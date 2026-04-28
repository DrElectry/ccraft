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
void world_destroy(World* world);
void world_render(World* world, void* active_program);

#endif