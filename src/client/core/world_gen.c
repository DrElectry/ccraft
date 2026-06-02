#include "core/world_gen.h"
#include "core/world.h"
#include "core/chunk.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include "core/light.h"

#define GEN_WORKERS 8
#define WORK_QUEUE_CAP 64

typedef enum {
    JOB_GEN,
    JOB_MESH,
} JobKind;

typedef struct {
    uint16_t center[CHUNK_BLOCK_COUNT];
    ChunkNeighbors neighbors;
    uint32_t generation;
} MeshJobPayload;

typedef struct {
    JobKind kind;
    int cx;
    int cz;
    MeshJobPayload* mesh;
} WorkJob;

typedef struct {
    JobKind kind;
    int cx;
    int cz;
    uint16_t* gen_data;
    ChunkMeshResult* mesh;
} WorkResult;

static pthread_t workers[GEN_WORKERS];
static pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t work_cond = PTHREAD_COND_INITIALIZER;

static WorkJob job_queue[WORK_QUEUE_CAP];
static int job_head;
static int job_tail;
static int job_count;

static WorkResult result_queue[WORK_QUEUE_CAP];
static int result_head;
static int result_tail;
static int result_count;

static int work_running;
static int work_shutdown;

static void queue_push_job(WorkJob job) {
    job_queue[job_tail] = job;
    job_tail = (job_tail + 1) % WORK_QUEUE_CAP;
    job_count++;
}

static int queue_pop_job(WorkJob* out) {
    if (job_count <= 0)
        return 0;
    *out = job_queue[job_head];
    job_head = (job_head + 1) % WORK_QUEUE_CAP;
    job_count--;
    return 1;
}

static void queue_push_result(WorkResult res) {
    result_queue[result_tail] = res;
    result_tail = (result_tail + 1) % WORK_QUEUE_CAP;
    result_count++;
}

static int queue_pop_result(WorkResult* out) {
    if (result_count <= 0)
        return 0;
    *out = result_queue[result_head];
    result_head = (result_head + 1) % WORK_QUEUE_CAP;
    result_count--;
    return 1;
}

static int coords_in_job_queue(int cx, int cz, JobKind kind) {
    for (int i = 0; i < job_count; i++) {
        int idx = (job_head + i) % WORK_QUEUE_CAP;
        if (job_queue[idx].kind == kind &&
            job_queue[idx].cx == cx && job_queue[idx].cz == cz)
            return 1;
    }
    return 0;
}

static int coords_in_result_queue(int cx, int cz, JobKind kind) {
    for (int i = 0; i < result_count; i++) {
        int idx = (result_head + i) % WORK_QUEUE_CAP;
        if (result_queue[idx].kind == kind &&
            result_queue[idx].cx == cx && result_queue[idx].cz == cz)
            return 1;
    }
    return 0;
}

static int coords_in_flight(int cx, int cz, JobKind kind) {
    return coords_in_job_queue(cx, cz, kind) || coords_in_result_queue(cx, cz, kind);
}

static void discard_job(WorkJob* job) {
    if (job->kind == JOB_MESH && job->mesh)
        free(job->mesh);
}

static void discard_result(WorkResult* res) {
    if (res->kind == JOB_GEN)
        free(res->gen_data);
    else
        chunk_mesh_result_free(res->mesh);
}

static void* worker_main(void* arg) {
    (void)arg;

    while (1) {
        WorkJob job;

        pthread_mutex_lock(&queue_mutex);
        while (!work_shutdown && job_count <= 0)
            pthread_cond_wait(&work_cond, &queue_mutex);

        if (work_shutdown && job_count <= 0) {
            pthread_mutex_unlock(&queue_mutex);
            break;
        }

        queue_pop_job(&job);
        pthread_mutex_unlock(&queue_mutex);

        WorkResult res;
        res.cx = job.cx;
        res.cz = job.cz;
        res.kind = job.kind;

        if (job.kind == JOB_GEN) {
            Chunk chunk;
            chunk_generate(&chunk, job.cx, job.cz);
            res.gen_data = chunk.data;
            res.mesh = NULL;
        } else {
            ChunkMeshResult* mesh = (ChunkMeshResult*)calloc(1, sizeof(ChunkMeshResult));
            uint32_t gen = job.mesh->generation;
            mesh->sky_light = (uint8_t*)calloc(CHUNK_BLOCK_COUNT, 1);
            mesh->block_light = (uint8_t*)calloc(CHUNK_BLOCK_COUNT, 1);
            if (mesh->sky_light && mesh->block_light) {
                chunk_light_compute_padded(NULL, NULL, mesh->sky_light, mesh->block_light,
                                           &job.mesh->neighbors, job.cx, job.cz);
                chunk_build_mesh(mesh, job.mesh->center, mesh->sky_light, mesh->block_light,
                                 &job.mesh->neighbors, job.cx, job.cz);
            } else {
                free(mesh->sky_light);
                free(mesh->block_light);
                mesh->sky_light = NULL;
                mesh->block_light = NULL;
                chunk_build_mesh(mesh, job.mesh->center, NULL, NULL,
                                 &job.mesh->neighbors, job.cx, job.cz);
            }
            free(job.mesh);
            mesh->generation = gen;
            res.mesh = mesh;
            res.gen_data = NULL;
        }

        pthread_mutex_lock(&queue_mutex);
        if (result_count < WORK_QUEUE_CAP)
            queue_push_result(res);
        else
            discard_result(&res);
        pthread_mutex_unlock(&queue_mutex);
    }

    return NULL;
}

void world_gen_start(void) {
    if (work_running)
        return;

    job_head = job_tail = job_count = 0;
    result_head = result_tail = result_count = 0;
    work_shutdown = 0;
    work_running = 1;

    for (int i = 0; i < GEN_WORKERS; i++)
        pthread_create(&workers[i], NULL, worker_main, NULL);
}

void world_gen_stop(void) {
    if (!work_running)
        return;

    pthread_mutex_lock(&queue_mutex);
    work_shutdown = 1;
    pthread_cond_broadcast(&work_cond);
    pthread_mutex_unlock(&queue_mutex);

    for (int i = 0; i < GEN_WORKERS; i++)
        pthread_join(workers[i], NULL);

    pthread_mutex_lock(&queue_mutex);
    while (job_count > 0) {
        WorkJob job;
        queue_pop_job(&job);
        discard_job(&job);
    }
    while (result_count > 0) {
        WorkResult res;
        queue_pop_result(&res);
        discard_result(&res);
    }
    pthread_mutex_unlock(&queue_mutex);

    work_running = 0;
    work_shutdown = 0;
}

int world_gen_in_flight(int cx, int cz) {
    pthread_mutex_lock(&queue_mutex);
    int found = coords_in_flight(cx, cz, JOB_GEN);
    pthread_mutex_unlock(&queue_mutex);
    return found;
}

int world_gen_submit(int cx, int cz) {
    int ok = 0;

    pthread_mutex_lock(&queue_mutex);
    if (job_count < WORK_QUEUE_CAP && !coords_in_flight(cx, cz, JOB_GEN)) {
        WorkJob job = {.kind = JOB_GEN, .cx = cx, .cz = cz, .mesh = NULL};
        queue_push_job(job);
        pthread_cond_signal(&work_cond);
        ok = 1;
    }
    pthread_mutex_unlock(&queue_mutex);

    return ok;
}

// pls work
int world_gen_poll(int* cx, int* cz, uint16_t** data) {
    int ok = 0;
    WorkResult deferred[WORK_QUEUE_CAP];
    int deferred_count = 0;

    pthread_mutex_lock(&queue_mutex);
    while (result_count > 0) {
        WorkResult res;
        queue_pop_result(&res);
        if (!ok && res.kind == JOB_GEN) {
            *cx = res.cx;
            *cz = res.cz;
            *data = res.gen_data;
            ok = 1;
        } else {
            deferred[deferred_count++] = res;
        }
    }
    for (int i = 0; i < deferred_count; i++)
        queue_push_result(deferred[i]);
    pthread_mutex_unlock(&queue_mutex);

    return ok;
}

int world_mesh_in_flight(int cx, int cz) {
    pthread_mutex_lock(&queue_mutex);
    int found = coords_in_flight(cx, cz, JOB_MESH);
    pthread_mutex_unlock(&queue_mutex);
    return found;
}

int world_mesh_submit(World* world, int cx, int cz) {
    Chunk* chunk = world_get_chunk(world, cx, cz);
    if (!chunk || !chunk->data)
        return 0;

    MeshJobPayload* payload = (MeshJobPayload*)malloc(sizeof(MeshJobPayload));
    if (!payload)
        return 0;

    memcpy(payload->center, chunk->data, CHUNK_BLOCK_COUNT * sizeof(uint16_t));
    chunk_neighbors_capture(&payload->neighbors, world, cx, cz);

    int ok = 0;
    pthread_mutex_lock(&queue_mutex);
    if (job_count < WORK_QUEUE_CAP && !coords_in_flight(cx, cz, JOB_MESH)) {
        payload->generation = chunk->mesh_generation;
        WorkJob job = {.kind = JOB_MESH, .cx = cx, .cz = cz, .mesh = payload};
        queue_push_job(job);
        pthread_cond_signal(&work_cond);
        ok = 1;
    } else {
        free(payload);
    }
    pthread_mutex_unlock(&queue_mutex);

    return ok;
}

int world_mesh_poll(int* cx, int* cz, ChunkMeshResult** mesh, uint32_t* generation) {
    int ok = 0;
    WorkResult deferred[WORK_QUEUE_CAP];
    int deferred_count = 0;

    pthread_mutex_lock(&queue_mutex);
    while (result_count > 0) {
        WorkResult res;
        queue_pop_result(&res);
        if (!ok && res.kind == JOB_MESH) {
            *cx = res.cx;
            *cz = res.cz;
            *mesh = res.mesh;
            *generation = res.mesh->generation;
            ok = 1;
        } else {
            deferred[deferred_count++] = res;
        }
    }
    for (int i = 0; i < deferred_count; i++)
        queue_push_result(deferred[i]);
    pthread_mutex_unlock(&queue_mutex);

    return ok;
}
