#ifndef CHUNK_H
#define CHUNK_H

#include <stdint.h> 
#include "gfx.h"
#include <cglm/cglm.h>

#define CHUNK_WIDTH 16
#define CHUNK_HEIGHT 64 // for now, later will be 256
#define CHUNK_DEPTH 16

typedef enum : uint16_t {
    AIR = 0,
    GRASS = 1,
    DIRT = 2,
    LEAVES = 3,
    STONE = 4,
    IRON_BLOCK = 5,
    WATER = 6,
    LOG = 7,
} Tile;

typedef struct {
    uint16_t* data; // 2 bytes for block id meaning that we have 65535 possible unique blocks

    Render_request model; // has a pos, rot, scale, and GPU buffers inside, gfx.h does everything else
    Render_request water_model;
} Chunk;

struct World;

void chunk_set_position(int cx, int cz);
void chunk_generate(Chunk* chunk);
void chunk_rebuild(Chunk* chunk, struct World* world, int cx, int cz);

#endif
