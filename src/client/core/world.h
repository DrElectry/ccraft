#ifndef WORLD_H
#define WORLD_H

#include "core/chunk.h"
#include "utils/file.h"
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

#ifndef FAR_WATER_DISTANCE
#define FAR_WATER_DISTANCE 4
#endif

typedef struct BlockChange {
    int x;
    int y;
    int z;
    uint16_t block;
} BlockChange;

typedef struct {
    int idx;
    float dist2;
} WaterSortEntry; // for sorting in potato mode

typedef struct World {
    Chunk* chunks_map;
    vec2* positions_map;
    int* index_map;

    vec2 pending_rebuilds[MAX_PENDING_REBUILDS];
    int pending_count;

    BlockChange* pending_block_changes;
    int pending_block_count;
    int pending_block_capacity;
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

void world_queue_block_change(World* world, int x, int y, int z, uint16_t block);

void world_render(World* world, void* active_program, void* water_program, int cull, int render_water);
void world_render_water(World* world, void* program, int cull, void* background_tex); // nice
void world_render_water_only(World* world, void* program, int cull);
void world_tick(World* world, vec3 ppos);

void world_reload_render_distance(World* world, vec3 ppos);

void world_load(World* world, File* file);
void world_save(World* world, const char* filename);

void rebuild_chunks_for_block(World* world, int wx, int wy, int wz);
void new_world(const char* filename);

#endif