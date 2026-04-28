#include <stdlib.h>
#include <stdio.h>
#include "world.h"

void world_init(World* world) {
    world->chunks_map = malloc(sizeof(Chunk) * MAX_LOADED_CHUNKS);
    world->positions_map = malloc(sizeof(vec2) * MAX_LOADED_CHUNKS);
    world->index_map = malloc(sizeof(int) * MAX_LOADED_CHUNKS);

    for (int i = 0; i < MAX_LOADED_CHUNKS; i++) {
        world->index_map[i] = -1;
    }
}

void world_add_chunk(World* world, Chunk* chunk, vec2 position) {
    for (int i = 0; i < MAX_LOADED_CHUNKS; i++) {
        if (world->index_map[i] != -1) {
            if (world->positions_map[i][0] == position[0] &&
                world->positions_map[i][1] == position[1]) {
                world->chunks_map[i] = *chunk;
                return;
            }
        }
    }

    for (int i = 0; i < MAX_LOADED_CHUNKS; i++) {
        if (world->index_map[i] == -1) {
            world->chunks_map[i] = *chunk;
            world->positions_map[i][0] = position[0];
            world->positions_map[i][1] = position[1];
            world->index_map[i] = 1;

            world->chunks_map[i].model.pos[0] = position[0] * CHUNK_WIDTH;
            world->chunks_map[i].model.pos[1] = 0.0f;
            world->chunks_map[i].model.pos[2] = position[1] * CHUNK_DEPTH;
            return;
        }
    }

    printf("Cant add new chunks, world overflow.\n");
}

static inline int floor_div(int a, int b) {
    if (a >= 0) return a / b;
    return - ((-a + b - 1) / b);
}

static inline int floor_mod(int a, int b) {
    int r = a % b;
    if (r < 0) r += b;
    return r;
}

Chunk* world_get_chunk(World* world, int cx, int cz) {
    for (int i = 0; i < MAX_LOADED_CHUNKS; i++) {
        if (world->index_map[i] != -1) {
            if ((int)world->positions_map[i][0] == cx &&
                (int)world->positions_map[i][1] == cz) {
                return &world->chunks_map[i];
            }
        }
    }
    return NULL;
}

uint16_t world_get_block(World* world, int wx, int wy, int wz) {
    if (wy < 0 || wy >= CHUNK_HEIGHT)
        return AIR;

    int cx = floor_div(wx, CHUNK_WIDTH);
    int cz = floor_div(wz, CHUNK_DEPTH);

    Chunk* chunk = world_get_chunk(world, cx, cz);
    if (!chunk)
        return AIR;

    int lx = floor_mod(wx, CHUNK_WIDTH);
    int lz = floor_mod(wz, CHUNK_DEPTH);

    int index = lx + CHUNK_WIDTH * (wy + CHUNK_HEIGHT * lz);
    return chunk->data[index];
}

uint16_t world_get_block_at(World* world, vec3 p) {
    int wx = (int)floorf(p[0]);
    int wy = (int)floorf(p[1]);
    int wz = (int)floorf(p[2]);

    return world_get_block(world, wx, wy, wz);
}

int world_set_block(World* world, int wx, int wy, int wz, uint16_t block) {
    if (wy < 0 || wy >= CHUNK_HEIGHT)
        return 0;

    int cx = floor_div(wx, CHUNK_WIDTH);
    int cz = floor_div(wz, CHUNK_DEPTH);

    Chunk* chunk = world_get_chunk(world, cx, cz);
    if (!chunk)
        return 0;

    int lx = floor_mod(wx, CHUNK_WIDTH);
    int lz = floor_mod(wz, CHUNK_DEPTH);

    int index = lx + CHUNK_WIDTH * (wy + CHUNK_HEIGHT * lz);
    chunk->data[index] = block;
    return 1;
}

void world_set_block_at(World* world, vec3 p, uint16_t block) {
    int wx = (int)floorf(p[0]);
    int wy = (int)floorf(p[1]);
    int wz = (int)floorf(p[2]);

    world_set_block(world, wx, wy, wz, block);
}

void world_rebuild_chunk(World* world, int cx, int cz) {
    Chunk* chunk = world_get_chunk(world, cx, cz);
    if (chunk) {
        chunk_rebuild(chunk, world, cx, cz);
    }
}

void world_render(World* world, void* active_program) {
    for (int i = 0; i < MAX_LOADED_CHUNKS; i++) {
        if (world->index_map[i] != -1) {
            gfx_render(&world->chunks_map[i].model, (Program*)active_program);
        }
    }
}

void world_destroy(World* world) {
    for (int i = 0; i < MAX_LOADED_CHUNKS; i++) {
        if (world->index_map[i] != -1) {
            Chunk* chunk = &world->chunks_map[i];
            if (chunk->model.data) {
                free(chunk->model.data);
                chunk->model.data = NULL;
            }
            if (chunk->model.triangles) {
                free(chunk->model.triangles);
                chunk->model.triangles = NULL;
            }
            vbo_free(&chunk->model.cache.vbo);
            vao_free(&chunk->model.cache.vao);
            ebo_free(&chunk->model.cache.ebo);
        }
    }

    free(world->chunks_map);
    free(world->positions_map);
    free(world->index_map);

    world->chunks_map = NULL;
    world->positions_map = NULL;
    world->index_map = NULL;
}

