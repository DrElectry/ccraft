#include "world.h"
#include <stdlib.h>
#include <stdio.h>

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

