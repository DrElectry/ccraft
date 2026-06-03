#ifndef CHUNK_H
#define CHUNK_H

#include <stdint.h> 
#include "core/gfx.h"
#include <cglm/cglm.h>

#define CHUNK_WIDTH 32
#define CHUNK_HEIGHT 128
#define CHUNK_DEPTH 32

typedef enum : uint16_t {
    AIR = 0,
    GRASS = 1,
    DIRT = 2,
    LEAVES = 3,
    STONE = 4,
    IRON_BLOCK = 5,
    WATER = 6,
    LOG = 7,
    GLASS = 8,
    COAL_ORE = 9,
    IRON_ORE = 10, // was copper before but i decided to rename to iron
    GOLD_ORE = 11,
    SAND = 12,
    GRAVEL = 13,
    LAVA = 14,
    ROSE = 15,
    GRASS_CROSS = 16,
    BORDER = 17,
    PLANKS = 18,
} Tile;

#define FIRST_TILE 1 // so we dont place air on the right button press

#define LAST_TILE 18

typedef struct {
    uint16_t* data; // 2 bytes for block id meaning that we have 65535 possible unique blocks
    uint8_t* sky_light;
    uint8_t* block_light;
    uint32_t mesh_generation;

    Render_request model; // has a pos, rot, scale, and GPU buffers inside, gfx.h does everything else
    Render_request water_model;
} Chunk;

#define CHUNK_BLOCK_COUNT (CHUNK_WIDTH * CHUNK_HEIGHT * CHUNK_DEPTH)
#define CHUNK_VERT_FLOATS 9

typedef struct {
    int cx;
    int cz;
    uint16_t chunks[3][3][CHUNK_BLOCK_COUNT];
    int loaded[3][3];
} ChunkNeighbors;

typedef struct {
    float* model_verts;
    int* model_inds;
    int model_v;
    int model_i;
    float* water_verts;
    int* water_inds;
    int water_v;
    int water_i;
    uint8_t* sky_light;
    uint8_t* block_light;
    uint32_t generation;
} ChunkMeshResult;

struct World;

extern int lookup_ignorecollision[];
extern int lookup_atlas[];

void chunk_generate(Chunk* chunk, int cx, int cz);
void chunk_neighbors_capture(ChunkNeighbors* n, struct World* world, int cx, int cz);
uint16_t chunk_neighbors_get(const ChunkNeighbors* n, int wx, int wy, int wz);
void chunk_build_mesh(ChunkMeshResult* out, const uint16_t* data,
                      const uint8_t* sky_light, const uint8_t* block_light,
                      const ChunkNeighbors* n, int cx, int cz);
void chunk_apply_mesh(Chunk* chunk, ChunkMeshResult* mesh);
void chunk_mesh_result_free(ChunkMeshResult* mesh);
uint16_t chunk_sound_pack(uint16_t tile_id);

#endif