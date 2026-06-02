#include "core/world_gen.h"
#include "core/chunk.h"
#include <pthread.h>
#include <stdlib.h>

#define GEN_WORKERS 4
#define GEN_QUEUE_CAP 128

typedef struct {
    int cx;
    int cz;
} GenJob;

typedef struct {
    int cx;
    int cz;
    uint16_t* data;
} GenResult;

static pthread_t workers[GEN_WORKERS];
static pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t work_cond = PTHREAD_COND_INITIALIZER;

static GenJob job_queue[GEN_QUEUE_CAP];
static int job_head;
static int job_tail;
static int job_count;

static GenResult result_queue[GEN_QUEUE_CAP];
static int result_head;
static int result_tail;
static int result_count;

static int gen_running;
static int gen_shutdown;

static void queue_push_job(int cx, int cz) {
    job_queue[job_tail].cx = cx;
    job_queue[job_tail].cz = cz;
    job_tail = (job_tail + 1) % GEN_QUEUE_CAP;
    job_count++;
}

static int queue_pop_job(GenJob* out) {
    if (job_count <= 0)
        return 0;
    *out = job_queue[job_head];
    job_head = (job_head + 1) % GEN_QUEUE_CAP;
    job_count--;
    return 1;
}

static void queue_push_result(int cx, int cz, uint16_t* data) {
    result_queue[result_tail].cx = cx;
    result_queue[result_tail].cz = cz;
    result_queue[result_tail].data = data;
    result_tail = (result_tail + 1) % GEN_QUEUE_CAP;
    result_count++;
}

static int queue_pop_result(GenResult* out) {
    if (result_count <= 0)
        return 0;
    *out = result_queue[result_head];
    result_head = (result_head + 1) % GEN_QUEUE_CAP;
    result_count--;
    return 1;
}

static int coords_in_job_queue(int cx, int cz) {
    for (int i = 0; i < job_count; i++) {
        int idx = (job_head + i) % GEN_QUEUE_CAP;
        if (job_queue[idx].cx == cx && job_queue[idx].cz == cz)
            return 1;
    }
    return 0;
}

static int coords_in_result_queue(int cx, int cz) {
    for (int i = 0; i < result_count; i++) {
        int idx = (result_head + i) % GEN_QUEUE_CAP;
        if (result_queue[idx].cx == cx && result_queue[idx].cz == cz)
            return 1;
    }
    return 0;
}

static void* worker_main(void* arg) {
    (void)arg;

    while (1) {
        GenJob job;

        pthread_mutex_lock(&queue_mutex);
        while (!gen_shutdown && job_count <= 0)
            pthread_cond_wait(&work_cond, &queue_mutex);

        if (gen_shutdown && job_count <= 0) {
            pthread_mutex_unlock(&queue_mutex);
            break;
        }

        queue_pop_job(&job);
        pthread_mutex_unlock(&queue_mutex);

        Chunk chunk;
        chunk_generate(&chunk, job.cx, job.cz);

        pthread_mutex_lock(&queue_mutex);
        if (result_count < GEN_QUEUE_CAP)
            queue_push_result(job.cx, job.cz, chunk.data);
        else
            free(chunk.data);
        pthread_mutex_unlock(&queue_mutex);
    }

    return NULL;
}

void world_gen_start(void) {
    if (gen_running)
        return;

    job_head = job_tail = job_count = 0;
    result_head = result_tail = result_count = 0;
    gen_shutdown = 0;
    gen_running = 1;

    for (int i = 0; i < GEN_WORKERS; i++)
        pthread_create(&workers[i], NULL, worker_main, NULL);
}

void world_gen_stop(void) {
    if (!gen_running)
        return;

    pthread_mutex_lock(&queue_mutex);
    gen_shutdown = 1;
    pthread_cond_broadcast(&work_cond);
    pthread_mutex_unlock(&queue_mutex);

    for (int i = 0; i < GEN_WORKERS; i++)
        pthread_join(workers[i], NULL);

    pthread_mutex_lock(&queue_mutex);
    while (job_count > 0) {
        GenJob job;
        queue_pop_job(&job);
    }
    while (result_count > 0) {
        GenResult res;
        queue_pop_result(&res);
        free(res.data);
    }
    pthread_mutex_unlock(&queue_mutex);

    gen_running = 0;
    gen_shutdown = 0;
}

int world_gen_in_flight(int cx, int cz) {
    pthread_mutex_lock(&queue_mutex);
    int found = coords_in_job_queue(cx, cz) || coords_in_result_queue(cx, cz);
    pthread_mutex_unlock(&queue_mutex);
    return found;
}

int world_gen_submit(int cx, int cz) {
    int ok = 0;

    pthread_mutex_lock(&queue_mutex);
    if (job_count < GEN_QUEUE_CAP &&
        !coords_in_job_queue(cx, cz) &&
        !coords_in_result_queue(cx, cz)) {
        queue_push_job(cx, cz);
        pthread_cond_signal(&work_cond);
        ok = 1;
    }
    pthread_mutex_unlock(&queue_mutex);

    return ok;
}

int world_gen_poll(int* cx, int* cz, uint16_t** data) {
    int ok = 0;

    pthread_mutex_lock(&queue_mutex);
    if (result_count > 0) {
        GenResult res;
        queue_pop_result(&res);
        *cx = res.cx;
        *cz = res.cz;
        *data = res.data;
        ok = 1;
    }
    pthread_mutex_unlock(&queue_mutex);

    return ok;
}
