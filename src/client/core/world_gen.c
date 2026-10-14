#include "core/world_gen.h"
#include "core/world.h"
#include "core/chunk.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "core/light.h"
#include <stdint.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <process.h>

typedef HANDLE thread_t;
typedef SRWLOCK work_mutex_t;
typedef CONDITION_VARIABLE work_cond_t;

#define WORK_MUTEX_INIT SRWLOCK_INIT
#define WORK_COND_INIT  CONDITION_VARIABLE_INIT

static void work_mutex_lock(work_mutex_t* m) {
    AcquireSRWLockExclusive(m);
}

static void work_mutex_unlock(work_mutex_t* m) {
    ReleaseSRWLockExclusive(m);
}

static void work_cond_wait(work_cond_t* c, work_mutex_t* m) {
    SleepConditionVariableSRW(c, m, INFINITE, 0);
}

static void work_cond_signal(work_cond_t* c) {
    WakeConditionVariable(c);
}

static void work_cond_broadcast(work_cond_t* c) {
    WakeAllConditionVariable(c);
}

typedef unsigned (__stdcall *thread_func_t)(void*);

static int work_thread_create(thread_t* out, thread_func_t fn, void* arg) {
    uintptr_t h = _beginthreadex(NULL, 0, fn, arg, 0, NULL);
    if (h == 0)
        return 0;

    *out = (HANDLE)h;
    return 1;
}

static void work_thread_join(thread_t t) {
    WaitForSingleObject(t, INFINITE);
    CloseHandle(t);
}

static int work_get_cpu_count(void) {
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return (info.dwNumberOfProcessors > 0)
        ? (int)info.dwNumberOfProcessors
        : 1;
}

#else
#include <pthread.h>
#include <unistd.h>

typedef pthread_t thread_t;
typedef pthread_mutex_t work_mutex_t;
typedef pthread_cond_t work_cond_t;

#define WORK_MUTEX_INIT PTHREAD_MUTEX_INITIALIZER
#define WORK_COND_INIT  PTHREAD_COND_INITIALIZER

static void work_mutex_lock(work_mutex_t* m) {
    pthread_mutex_lock(m);
}

static void work_mutex_unlock(work_mutex_t* m) {
    pthread_mutex_unlock(m);
}

static void work_cond_wait(work_cond_t* c, work_mutex_t* m) {
    pthread_cond_wait(c, m);
}

static void work_cond_signal(work_cond_t* c) {
    pthread_cond_signal(c);
}

static void work_cond_broadcast(work_cond_t* c) {
    pthread_cond_broadcast(c);
}

typedef void* (*thread_func_t)(void*);

static int work_thread_create(thread_t* out, thread_func_t fn, void* arg) {
    return pthread_create(out, NULL, fn, arg) == 0;
}

static void work_thread_join(thread_t t) {
    pthread_join(t, NULL);
}

static int work_get_cpu_count(void) {
    long cores = sysconf(_SC_NPROCESSORS_ONLN);
    return (cores > 0) ? (int)cores : 1;
}
#endif

static thread_t workers[8];
static int num_workers = 0;
static work_mutex_t queue_mutex = WORK_MUTEX_INIT;
static work_cond_t work_cond = WORK_COND_INIT;

static unsigned long worker_gen_count[8] = {0};
static unsigned long worker_mesh_count[8] = {0};
static unsigned long mesh_busy_skips = 0;
static unsigned long result_queue_waits = 0;

static WorkJob gen_job_queue[WORK_QUEUE_CAP];
static int gen_job_head = 0, gen_job_tail = 0, gen_job_count = 0;

static WorkJob mesh_job_queue[WORK_QUEUE_CAP];
static int mesh_job_head = 0, mesh_job_tail = 0, mesh_job_count = 0;

static WorkResult gen_result_queue[WORK_QUEUE_CAP];
static int gen_head = 0, gen_tail = 0, gen_count = 0;

static WorkResult mesh_result_queue[WORK_QUEUE_CAP];
static int mesh_head = 0, mesh_tail = 0, mesh_count = 0;

static int work_running = 0;
static int work_shutdown = 0;

static int player_chunk_cx = 0;
static int player_chunk_cz = 0;
static int mesh_first = 1;

#define INFLIGHT_TABLE_SIZE 1024
typedef struct { int cx; int cz; int occupied; } InflightEntry;
static InflightEntry gen_inflight[INFLIGHT_TABLE_SIZE];
static InflightEntry mesh_inflight[INFLIGHT_TABLE_SIZE];

static uint32_t hash_coord(int cx, int cz) {
    /*
     * Convert to unsigned before arithmetic so extreme/negative chunk
     * coordinates do not invoke signed-overflow UB.
     */
    uint32_t x = (uint32_t)cx;
    uint32_t z = (uint32_t)cz;
    uint32_t h = x * 0x9E3779B1u;
    h ^= z + 0x85EBCA6Bu + (h << 6) + (h >> 2);
    return h & (INFLIGHT_TABLE_SIZE - 1);
}

static InflightEntry* inflight_table(int kind) {
    return kind == JOB_GEN ? gen_inflight : mesh_inflight;
}

static int inflight_add(int cx, int cz, int kind) {
    InflightEntry* table = inflight_table(kind);
    uint32_t h = hash_coord(cx, cz);

    for (int i = 0; i < INFLIGHT_TABLE_SIZE; i++) {
        int idx = (int)((h + (uint32_t)i) & (INFLIGHT_TABLE_SIZE - 1));
        if (!table[idx].occupied) {
            table[idx].cx = cx;
            table[idx].cz = cz;
            table[idx].occupied = 1;
            return 1;
        }
        if (table[idx].cx == cx && table[idx].cz == cz)
            return 0;
    }

    return 0;
}

static void inflight_remove(int cx, int cz, int kind) {
    InflightEntry* table = inflight_table(kind);
    uint32_t h = hash_coord(cx, cz);

    for (int i = 0; i < INFLIGHT_TABLE_SIZE; i++) {
        int idx = (int)((h + (uint32_t)i) & (INFLIGHT_TABLE_SIZE - 1));
        if (!table[idx].occupied)
            return;
        if (table[idx].cx == cx && table[idx].cz == cz) {
            table[idx].occupied = 0;
            uint32_t next = ((uint32_t)idx + 1) & (INFLIGHT_TABLE_SIZE - 1);
            for (int moved = 0; moved < INFLIGHT_TABLE_SIZE - 1 && table[next].occupied; moved++) {
                InflightEntry entry = table[next];
                table[next].occupied = 0;
                inflight_add(entry.cx, entry.cz, kind);
                next = (next + 1) & (INFLIGHT_TABLE_SIZE - 1);
            }
            return;
        }
    }
}

static int inflight_contains(int cx, int cz, int kind) {
    InflightEntry* table = inflight_table(kind);
    uint32_t h = hash_coord(cx, cz);

    for (int i = 0; i < INFLIGHT_TABLE_SIZE; i++) {
        int idx = (int)((h + (uint32_t)i) & (INFLIGHT_TABLE_SIZE - 1));
        if (!table[idx].occupied)
            return 0;
        if (table[idx].cx == cx && table[idx].cz == cz)
            return 1;
    }

    return 0;
}

static int push_job(WorkJob job) {
    if (job.kind == JOB_GEN) {
        if (gen_job_count >= WORK_QUEUE_CAP)
            return 0;

        if (!inflight_add(job.cx, job.cz, JOB_GEN))
            return 0;

        gen_job_queue[gen_job_tail] = job;
        gen_job_tail = (gen_job_tail + 1) % WORK_QUEUE_CAP;
        gen_job_count++;
    } else {
        if (mesh_job_count >= WORK_QUEUE_CAP)
            return 0;

        if (!inflight_add(job.cx, job.cz, JOB_MESH))
            return 0;

        mesh_job_queue[mesh_job_tail] = job;
        mesh_job_tail = (mesh_job_tail + 1) % WORK_QUEUE_CAP;
        mesh_job_count++;
    }

    return 1;
}

static int job_is_cancelled(const WorkJob* job) {
    int limit = WORLD_RENDER_DISTANCE + 4;
    int dx = job->cx - player_chunk_cx;
    int dz = job->cz - player_chunk_cz;
    return (dx * dx + dz * dz) > limit * limit;
}

static void free_job_payload(WorkJob* job) {
    if (job->kind == JOB_MESH && job->mesh) {
        free(job->mesh);
        job->mesh = NULL;
    }
}

static int try_pop_one(WorkJob* out, int try_mesh) {
    if (try_mesh) {
        while (mesh_job_count > 0) {
            WorkJob job = mesh_job_queue[mesh_job_head];
            mesh_job_head = (mesh_job_head + 1) % WORK_QUEUE_CAP;
            mesh_job_count--;

            if (job_is_cancelled(&job)) {
                free_job_payload(&job);
                inflight_remove(job.cx, job.cz, job.kind);
                continue;
            }

            *out = job;
            return 1;
        }
    } else {
        while (gen_job_count > 0) {
            WorkJob job = gen_job_queue[gen_job_head];
            gen_job_head = (gen_job_head + 1) % WORK_QUEUE_CAP;
            gen_job_count--;

            if (job_is_cancelled(&job)) {
                inflight_remove(job.cx, job.cz, job.kind);
                continue;
            }

            *out = job;
            return 1;
        }
    }

    return 0;
}

static int pop_job(WorkJob* out) {
    for (int i = 0; i < 2; i++) {
        int want_mesh = mesh_first ^ i;
        if (try_pop_one(out, want_mesh)) {
            mesh_first = !mesh_first;
            return 1;
        }
    }

    return 0;
}

static void push_gen_result(WorkResult res) {
    gen_result_queue[gen_tail] = res;
    gen_tail = (gen_tail + 1) % WORK_QUEUE_CAP;
    gen_count++;
}

static int pop_gen_result(WorkResult* out) {
    if (gen_count <= 0)
        return 0;

    *out = gen_result_queue[gen_head];
    gen_head = (gen_head + 1) % WORK_QUEUE_CAP;
    gen_count--;

    inflight_remove(out->cx, out->cz, JOB_GEN);
    return 1;
}

static void push_mesh_result(WorkResult res) {
    mesh_result_queue[mesh_tail] = res;
    mesh_tail = (mesh_tail + 1) % WORK_QUEUE_CAP;
    mesh_count++;
}

static int pop_mesh_result(WorkResult* out) {
    if (mesh_count <= 0)
        return 0;

    *out = mesh_result_queue[mesh_head];
    mesh_head = (mesh_head + 1) % WORK_QUEUE_CAP;
    mesh_count--;

    inflight_remove(out->cx, out->cz, JOB_MESH);
    return 1;
}

static void discard_job(WorkJob* job) {
    free_job_payload(job);
    inflight_remove(job->cx, job->cz, job->kind);
}

static void discard_result(WorkResult* res) {
    if (res->kind == JOB_GEN) {
        if (res->gen_data)
            free(res->gen_data);
    } else {
        if (res->mesh)
            chunk_mesh_result_free(res->mesh);
    }

    inflight_remove(res->cx, res->cz, res->kind);
}

static void purge_gen_queue_locked(void) {
    int n = 0;

    for (int i = 0; i < gen_job_count; i++) {
        int idx = (gen_job_head + i) % WORK_QUEUE_CAP;
        WorkJob job = gen_job_queue[idx];

        if (job_is_cancelled(&job)) {
            inflight_remove(job.cx, job.cz, JOB_GEN);
        } else {
            gen_job_queue[(gen_job_head + n) % WORK_QUEUE_CAP] = job;
            n++;
        }
    }

    gen_job_tail = (gen_job_head + n) % WORK_QUEUE_CAP;
    gen_job_count = n;
}

static void purge_mesh_queue_locked(void) {
    int n = 0;

    for (int i = 0; i < mesh_job_count; i++) {
        int idx = (mesh_job_head + i) % WORK_QUEUE_CAP;
        WorkJob job = mesh_job_queue[idx];

        if (job_is_cancelled(&job)) {
            free_job_payload(&job);
            inflight_remove(job.cx, job.cz, JOB_MESH);
        } else {
            mesh_job_queue[(mesh_job_head + n) % WORK_QUEUE_CAP] = job;
            n++;
        }
    }

    mesh_job_tail = (mesh_job_head + n) % WORK_QUEUE_CAP;
    mesh_job_count = n;
}

#ifdef _WIN32
static unsigned __stdcall worker_main(void* arg)
#else
static void* worker_main(void* arg)
#endif
{
    int worker_idx = (int)(uintptr_t)arg;

    while (1) {
        WorkJob job;

        work_mutex_lock(&queue_mutex);

        while (!work_shutdown &&
               gen_job_count == 0 &&
               mesh_job_count == 0) {
            work_cond_wait(&work_cond, &queue_mutex);
        }

        if (work_shutdown &&
            gen_job_count == 0 &&
            mesh_job_count == 0) {
            work_mutex_unlock(&queue_mutex);
            break;
        }

        if (!pop_job(&job)) {
            work_mutex_unlock(&queue_mutex);
            continue;
        }

        if (job.kind == JOB_GEN)
            worker_gen_count[worker_idx]++;
        else
            worker_mesh_count[worker_idx]++;

        work_mutex_unlock(&queue_mutex);

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
            ChunkMeshResult* mesh =
                (ChunkMeshResult*)calloc(1, sizeof(ChunkMeshResult));

            if (!mesh) {
                free(job.mesh);
                work_mutex_lock(&queue_mutex);
                inflight_remove(job.cx, job.cz, job.kind);
                work_mutex_unlock(&queue_mutex);
                continue;
            }

            uint32_t gen = job.mesh->generation;

            mesh->sky_light =
                (uint8_t*)calloc(CHUNK_BLOCK_COUNT, 1);
            mesh->block_light =
                (uint8_t*)calloc(CHUNK_BLOCK_COUNT, 1);

            if (mesh->sky_light && mesh->block_light) {
                chunk_light_compute_padded(
                    NULL, NULL,
                    mesh->sky_light,
                    mesh->block_light,
                    &job.mesh->neighbors,
                    job.cx, job.cz
                );

                chunk_build_mesh(
                    mesh,
                    job.mesh->center,
                    mesh->sky_light,
                    mesh->block_light,
                    &job.mesh->neighbors,
                    job.cx, job.cz
                );
            } else {
                free(mesh->sky_light);
                free(mesh->block_light);
                mesh->sky_light = NULL;
                mesh->block_light = NULL;

                chunk_build_mesh(
                    mesh,
                    job.mesh->center,
                    NULL,
                    NULL,
                    &job.mesh->neighbors,
                    job.cx, job.cz
                );
            }

            free(job.mesh);
            mesh->generation = gen;
            res.mesh = mesh;
        }

        work_mutex_lock(&queue_mutex);

        while (!work_shutdown &&
               (res.kind == JOB_GEN ? gen_count : mesh_count) >= WORK_QUEUE_CAP) {
            result_queue_waits++;
            work_cond_wait(&work_cond, &queue_mutex);
        }

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

        work_mutex_unlock(&queue_mutex);
    }

#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

void world_gen_start(void) {
    if (work_running)
        return;

    int cores = work_get_cpu_count();
    if (cores < 1) cores = 1;

    int worker_count;

    if (cores <= 2) {
        worker_count = 1;
    } else if (cores <= 4) {
        worker_count = cores - 1;
    } else if (cores <= 8) {
        worker_count = cores - 1;
    } else if (cores <= 16) {
        worker_count = 6;
    } else {
        worker_count = 8;
    }

    if (worker_count < 1) worker_count = 1;
    if (worker_count > 8) worker_count = 8;

    num_workers = worker_count;

    printf("Chunk generation using %d worker threads (%d cores detected)\n",
           num_workers, cores);

    gen_job_head = gen_job_tail = gen_job_count = 0;
    mesh_job_head = mesh_job_tail = mesh_job_count = 0;
    gen_head = gen_tail = gen_count = 0;
    mesh_head = mesh_tail = mesh_count = 0;

    memset(gen_inflight, 0, sizeof(gen_inflight));
    memset(mesh_inflight, 0, sizeof(mesh_inflight));

    memset(worker_gen_count, 0, sizeof(worker_gen_count));
    memset(worker_mesh_count, 0, sizeof(worker_mesh_count));
    mesh_busy_skips = 0;
    result_queue_waits = 0;

    mesh_first = 1;
    player_chunk_cx = 0;
    player_chunk_cz = 0;

    work_shutdown = 0;
    work_running = 1;

    for (int i = 0; i < num_workers; i++) {
        if (!work_thread_create(&workers[i], worker_main, (void*)(uintptr_t)i)) {
            num_workers = i;
            work_shutdown = 1;
            work_mutex_lock(&queue_mutex);
            work_cond_broadcast(&work_cond);
            work_mutex_unlock(&queue_mutex);

            for (int j = 0; j < num_workers; j++)
                work_thread_join(workers[j]);

            work_running = 0;
            work_shutdown = 0;
            num_workers = 0;
            return;
        }
    }
}

void world_gen_stop(void) {
    if (!work_running)
        return;

    work_mutex_lock(&queue_mutex);
    work_shutdown = 1;
    work_cond_broadcast(&work_cond);
    work_mutex_unlock(&queue_mutex);

    for (int i = 0; i < num_workers; i++)
        work_thread_join(workers[i]);

    work_mutex_lock(&queue_mutex);

    while (gen_job_count > 0) {
        WorkJob job = gen_job_queue[gen_job_head];
        gen_job_head = (gen_job_head + 1) % WORK_QUEUE_CAP;
        gen_job_count--;
        discard_job(&job);
    }

    while (mesh_job_count > 0) {
        WorkJob job = mesh_job_queue[mesh_job_head];
        mesh_job_head = (mesh_job_head + 1) % WORK_QUEUE_CAP;
        mesh_job_count--;
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

    work_mutex_unlock(&queue_mutex);

    work_running = 0;
    work_shutdown = 0;
    num_workers = 0;
}

void world_gen_print_stats(void) {
    work_mutex_lock(&queue_mutex);

    unsigned long total_gen = 0;
    unsigned long total_mesh = 0;

    printf("---- chunk worker stats ----\n");

    for (int i = 0; i < num_workers; i++) {
        printf("  thread %d: gen=%lu mesh=%lu\n",
               i, worker_gen_count[i], worker_mesh_count[i]);
        total_gen += worker_gen_count[i];
        total_mesh += worker_mesh_count[i];
    }

    printf("  total:     gen=%lu mesh=%lu\n", total_gen, total_mesh);
    printf("  queued:    gen=%d mesh=%d\n", gen_job_count, mesh_job_count);
    printf("  results:   gen=%d mesh=%d\n", gen_count, mesh_count);
    printf("  mesh busy skips: %lu\n", mesh_busy_skips);
    printf("  result queue waits: %lu\n", result_queue_waits);

    work_mutex_unlock(&queue_mutex);
}

void world_gen_set_player_chunk(int cx, int cz) {
    work_mutex_lock(&queue_mutex);

    if (cx != player_chunk_cx || cz != player_chunk_cz) {
        player_chunk_cx = cx;
        player_chunk_cz = cz;

        purge_gen_queue_locked();
        purge_mesh_queue_locked();
    }

    work_mutex_unlock(&queue_mutex);
}

int world_gen_in_flight(int cx, int cz) {
    work_mutex_lock(&queue_mutex);
    int found = inflight_contains(cx, cz, JOB_GEN);
    work_mutex_unlock(&queue_mutex);
    return found;
}

int world_gen_submit(int cx, int cz) {
    int ok = 0;

    work_mutex_lock(&queue_mutex);

    if (gen_job_count < WORK_QUEUE_CAP &&
        !inflight_contains(cx, cz, JOB_GEN)) {
        WorkJob job = {
            .kind = JOB_GEN,
            .cx = cx,
            .cz = cz,
            .mesh = NULL
        };

        if (push_job(job)) {
            work_cond_signal(&work_cond);
            ok = 1;
        }
    }

    work_mutex_unlock(&queue_mutex);

    return ok;
}

int world_gen_poll(int* cx, int* cz, uint16_t** data) {
    int ok = 0;

    work_mutex_lock(&queue_mutex);

    if (gen_count > 0) {
        WorkResult res;
        pop_gen_result(&res);
        work_cond_broadcast(&work_cond);

        *cx = res.cx;
        *cz = res.cz;
        *data = res.gen_data;
        ok = 1;
    }

    work_mutex_unlock(&queue_mutex);

    return ok;
}

int world_mesh_in_flight(int cx, int cz) {
    work_mutex_lock(&queue_mutex);
    int found = inflight_contains(cx, cz, JOB_MESH);
    work_mutex_unlock(&queue_mutex);
    return found;
}

int world_mesh_submit(struct World* world, int cx, int cz) {
    Chunk* chunk = world_get_chunk(world, cx, cz);

    if (!chunk || !chunk->data)
        return 0;

    work_mutex_lock(&queue_mutex);
    int skip_snapshot = mesh_job_count >= WORK_QUEUE_CAP ||
        inflight_contains(cx, cz, JOB_MESH);
    if (skip_snapshot && mesh_job_count < WORK_QUEUE_CAP)
        mesh_busy_skips++;
    work_mutex_unlock(&queue_mutex);
    if (skip_snapshot)
        return 0;

    MeshJobPayload* payload =
        (MeshJobPayload*)malloc(sizeof(MeshJobPayload));

    if (!payload)
        return 0;

    memcpy(
        payload->center,
        chunk->data,
        CHUNK_BLOCK_COUNT * sizeof(uint16_t)
    );

    chunk_neighbors_capture(&payload->neighbors, world, cx, cz);

    int ok = 0;

    work_mutex_lock(&queue_mutex);

    if (mesh_job_count < WORK_QUEUE_CAP &&
        !inflight_contains(cx, cz, JOB_MESH)) {

        payload->generation = chunk->mesh_generation;

        WorkJob job = {
            .kind = JOB_MESH,
            .cx = cx,
            .cz = cz,
            .mesh = payload
        };

        if (push_job(job)) {
            work_cond_signal(&work_cond);
            ok = 1;
        } else {
            free(payload);
        }
    } else {
        free(payload);
    }

    work_mutex_unlock(&queue_mutex);

    return ok;
}

int world_mesh_poll(
    int* cx,
    int* cz,
    ChunkMeshResult** mesh,
    uint32_t* generation
) {
    int ok = 0;

    work_mutex_lock(&queue_mutex);

    if (mesh_count > 0) {
        WorkResult res;
        pop_mesh_result(&res);
        work_cond_broadcast(&work_cond);

        *cx = res.cx;
        *cz = res.cz;
        *mesh = res.mesh;
        *generation = res.mesh->generation;
        ok = 1;
    }

    work_mutex_unlock(&queue_mutex);

    return ok;
}