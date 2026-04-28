#ifndef CHUNK_H
#define CHUNK_H

#include <stdint.h> 
#include "gfx.h"
#include <cglm/cglm.h>

#define CHUNK_WIDTH 4
#define CHUNK_HEIGHT 4 // for now, later will be 256
#define CHUNK_DEPTH 4

typedef enum : uint16_t {
    AIR = 0,
    GRASS = 1,
} Tile;

typedef struct {
    uint16_t* data; // 2 bytes for block id meaning that we have 65535 possible unique blocks

    Render_request model; // has a pos, rot, scale, and GPU buffers inside, gfx.h does everything else
} Chunk;

void chunk_generate(Chunk* chunk);
void chunk_rebuild(Chunk* chunk);

#endif