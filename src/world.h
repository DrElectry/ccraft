#ifndef WORLD_H
#define WORLD_H

#include <cglm/cglm.h>
#include "chunk.h"

#ifndef MAX_LOADED_CHUNKS
#define MAX_LOADED_CHUNKS 1024
#endif

typedef struct World {
    Chunk* chunks_map;
    vec2* positions_map;
    int* index_map;
} World;

void world_init(World* world);
void world_add_chunk(World* world, Chunk* chunk, vec2 position);
Chunk* world_get_chunk(World* world, int cx, int cz);
uint16_t world_get_block(World* world, int wx, int wy, int wz);
void world_rebuild_chunk(World* world, int cx, int cz);
void world_destroy(World* world);
void world_render(World* world, void* active_program);

#endif
