#ifndef WORLD_H
#define WORLD_H

#include "chunk.h"
#include <cglm/cglm.h>
#include <stdint.h>

#ifndef MAX_LOADED_CHUNKS
#define MAX_LOADED_CHUNKS 1024
#endif

#ifndef MAX_PENDING_REBUILDS
#define MAX_PENDING_REBUILDS 256
#endif

#ifndef WORLD_RENDER_DISTANCE
#define WORLD_RENDER_DISTANCE 8
#endif

typedef struct World {
    Chunk* chunks_map;
    vec2* positions_map;
    int* index_map;

    // Chunk rebuild queue
    vec2 pending_rebuilds[MAX_PENDING_REBUILDS];
    int pending_count;

} World;

void world_init(World* world);
void world_destroy(World* world);

void world_add_chunk(World* world, Chunk* chunk, vec2 position);
Chunk* world_get_chunk(World* world, int cx, int cz);

uint16_t world_get_block(World* world, int wx, int wy, int wz);
uint16_t world_get_block_at(World* world, vec3 p);

int world_set_block(World* world, int wx, int wy, int wz, uint16_t block);
void world_set_block_at(World* world, vec3 p, uint16_t block);

void world_rebuild_chunk(World* world, int cx, int cz);

void world_queue_rebuild(World* world, int cx, int cz);
void world_process_rebuild_queue(World* world);

void world_render(World* world, void* active_program, void* water_program);
void world_tick(World* world, vec3 ppos);

#endif