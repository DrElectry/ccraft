#ifndef WORLD_GEN_H
#define WORLD_GEN_H

#include "core/chunk.h"
#include <stdint.h>

struct World;

#define GEN_WORKERS 8
#define WORK_QUEUE_CAP 256

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

void world_gen_start(void);
void world_gen_stop(void);

int world_gen_submit(int cx, int cz);
int world_gen_in_flight(int cx, int cz);
int world_gen_poll(int* cx, int* cz, uint16_t** data);

int world_mesh_submit(struct World* world, int cx, int cz);
int world_mesh_in_flight(int cx, int cz);
int world_mesh_poll(int* cx, int* cz, ChunkMeshResult** mesh, uint32_t* generation);

#endif