#include "core/world_gen.h"
#include "core/world.h"
#include "core/chunk.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "core/light.h"

static pthread_t workers[GEN_WORKERS];
static pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t work_cond = PTHREAD_COND_INITIALIZER;

static WorkJob job_queue[WORK_QUEUE_CAP];
static int job_head = 0, job_tail = 0, job_count = 0;

static WorkResult gen_result_queue[WORK_QUEUE_CAP];
static int gen_head = 0, gen_tail = 0, gen_count = 0;

static WorkResult mesh_result_queue[WORK_QUEUE_CAP];
static int mesh_head = 0, mesh_tail = 0, mesh_count = 0;

static int work_running = 0;
static int work_shutdown = 0;

static void queue_push_job(WorkJob job) {
    job_queue[job_tail] = job;
    job_tail = (job_tail + 1) % WORK_QUEUE_CAP;
    job_count++;
}

static int queue_pop_job(WorkJob* out) {
    if (job_count <= 0) return 0;
    *out = job_queue[job_head];
    job_head = (job_head + 1) % WORK_QUEUE_CAP;
    job_count--;
    return 1;
}

static void push_gen_result(WorkResult res) {
    gen_result_queue[gen_tail] = res;
    gen_tail = (gen_tail + 1) % WORK_QUEUE_CAP;
    gen_count++;
}

static int pop_gen_result(WorkResult* out) {
    if (gen_count <= 0) return 0;
    *out = gen_result_queue[gen_head];
    gen_head = (gen_head + 1) % WORK_QUEUE_CAP;
    gen_count--;
    return 1;
}

static void push_mesh_result(WorkResult res) {
    mesh_result_queue[mesh_tail] = res;
    mesh_tail = (mesh_tail + 1) % WORK_QUEUE_CAP;
    mesh_count++;
}

static int pop_mesh_result(WorkResult* out) {
    if (mesh_count <= 0) return 0;
    *out = mesh_result_queue[mesh_head];
    mesh_head = (mesh_head + 1) % WORK_QUEUE_CAP;
    mesh_count--;
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

static int coords_in_gen_result(int cx, int cz) {
    for (int i = 0; i < gen_count; i++) {
        int idx = (gen_head + i) % WORK_QUEUE_CAP;
        if (gen_result_queue[idx].cx == cx && gen_result_queue[idx].cz == cz)
            return 1;
    }
    return 0;
}

static int coords_in_mesh_result(int cx, int cz) {
    for (int i = 0; i < mesh_count; i++) {
        int idx = (mesh_head + i) % WORK_QUEUE_CAP;
        if (mesh_result_queue[idx].cx == cx && mesh_result_queue[idx].cz == cz)
            return 1;
    }
    return 0;
}

static int coords_in_flight(int cx, int cz, JobKind kind) {
    if (coords_in_job_queue(cx, cz, kind)) return 1;
    if (kind == JOB_GEN) return coords_in_gen_result(cx, cz);
    else return coords_in_mesh_result(cx, cz);
}

static void discard_job(WorkJob* job) {
    if (job->kind == JOB_MESH && job->mesh)
        free(job->mesh);
}

static void discard_result(WorkResult* res) {
    if (res->kind == JOB_GEN) {
        if (res->gen_data) free(res->gen_data);
    } else {
        if (res->mesh) chunk_mesh_result_free(res->mesh);
    }
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
        res.gen_data = NULL;
        res.mesh = NULL;

        if (job.kind == JOB_GEN) {
            Chunk chunk;
            chunk_generate(&chunk, job.cx, job.cz);
            res.gen_data = chunk.data;
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
        }

        pthread_mutex_lock(&queue_mutex);
        if (res.kind == JOB_GEN) {
            if (gen_count < WORK_QUEUE_CAP)
                push_gen_result(res);
            else
                discard_result(&res);
        } else {
            if (mesh_count < WORK_QUEUE_CAP)
                push_mesh_result(res);
            else
                discard_result(&res);
        }
        pthread_mutex_unlock(&queue_mutex);
    }

    return NULL;
}

void world_gen_start(void) {
    if (work_running) return;

    job_head = job_tail = job_count = 0;
    gen_head = gen_tail = gen_count = 0;
    mesh_head = mesh_tail = mesh_count = 0;
    work_shutdown = 0;
    work_running = 1;

    for (int i = 0; i < GEN_WORKERS; i++)
        pthread_create(&workers[i], NULL, worker_main, NULL);
}

void world_gen_stop(void) {
    if (!work_running) return;

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
    while (gen_count > 0) {
        WorkResult res;
        pop_gen_result(&res);
        discard_result(&res);
    }
    while (mesh_count > 0) {
        WorkResult res;
        pop_mesh_result(&res);
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

int world_gen_poll(int* cx, int* cz, uint16_t** data) {
    int ok = 0;

    pthread_mutex_lock(&queue_mutex);
    if (gen_count > 0) {
        WorkResult res;
        pop_gen_result(&res);
        *cx = res.cx;
        *cz = res.cz;
        *data = res.gen_data;
        ok = 1;
    }
    pthread_mutex_unlock(&queue_mutex);

    return ok;
}

int world_mesh_in_flight(int cx, int cz) {
    pthread_mutex_lock(&queue_mutex);
    int found = coords_in_flight(cx, cz, JOB_MESH);
    pthread_mutex_unlock(&queue_mutex);
    return found;
}

int world_mesh_submit(struct World* world, int cx, int cz) {
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

    pthread_mutex_lock(&queue_mutex);
    if (mesh_count > 0) {
        WorkResult res;
        pop_mesh_result(&res);
        *cx = res.cx;
        *cz = res.cz;
        *mesh = res.mesh;
        *generation = res.mesh->generation;
        ok = 1;
    }
    pthread_mutex_unlock(&queue_mutex);

    return ok;
}