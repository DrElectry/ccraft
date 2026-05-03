#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "world.h"
#include "rand.h"
#include "noise.h"

void world_init(World* world) {
    world->chunks_map = malloc(sizeof(Chunk) * MAX_LOADED_CHUNKS);
    world->positions_map = malloc(sizeof(vec2) * MAX_LOADED_CHUNKS);
    world->index_map = malloc(sizeof(int) * MAX_LOADED_CHUNKS);

    for (int i = 0; i < MAX_LOADED_CHUNKS; i++) {
        world->index_map[i] = -1;
    }

    world->pending_count = 0;
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

world->chunks_map[i].water_model.pos[0] = position[0] * CHUNK_WIDTH;
            world->chunks_map[i].water_model.pos[1] = 0.0f;
            world->chunks_map[i].water_model.pos[2] = position[1] * CHUNK_DEPTH;
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

void world_queue_rebuild(World* world, int cx, int cz) {
    if (world->pending_count >= MAX_PENDING_REBUILDS)
        return;

    for (int i = 0; i < world->pending_count; i++) {
        if ((int)world->pending_rebuilds[i][0] == cx &&
            (int)world->pending_rebuilds[i][1] == cz) {
            return;
        }
    }

    world->pending_rebuilds[world->pending_count][0] = (float)cx;
    world->pending_rebuilds[world->pending_count][1] = (float)cz;
    world->pending_count++;
}

void world_process_rebuild_queue(World* world) {
    if (world->pending_count <= 0)
        return;

    int cx = (int)world->pending_rebuilds[0][0];
    int cz = (int)world->pending_rebuilds[0][1];

    world_rebuild_chunk(world, cx, cz);

    for (int i = 0; i < world->pending_count - 1; i++) {
        world->pending_rebuilds[i][0] = world->pending_rebuilds[i + 1][0];
        world->pending_rebuilds[i][1] = world->pending_rebuilds[i + 1][1];
    }
    world->pending_count--;
}

void world_rebuild_chunk(World* world, int cx, int cz) {
    Chunk* chunk = world_get_chunk(world, cx, cz);
    if (chunk) {
        chunk_rebuild(chunk, world, cx, cz);
    }
}

void world_render(World* world, void* active_program, void* water_program) {
    for (int i = 0; i < MAX_LOADED_CHUNKS; i++) {
        if (world->index_map[i] != -1) {
            gfx_render(&world->chunks_map[i].model, (Program*)active_program);
            gfx_render(&world->chunks_map[i].water_model, (Program*)water_program);
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

            if (chunk->water_model.data) {
                free(chunk->water_model.data);
                chunk->water_model.data = NULL;
            }
            if (chunk->water_model.triangles) {
                free(chunk->water_model.triangles);
                chunk->water_model.triangles = NULL;
            }
vbo_free(&chunk->water_model.cache.vbo);
            vao_free(&chunk->water_model.cache.vao);
            ebo_free(&chunk->water_model.cache.ebo);
        }
    }

    free(world->chunks_map);
    free(world->positions_map);
    free(world->index_map);

    world->chunks_map = NULL;
    world->positions_map = NULL;
    world->index_map = NULL;
}

void world_tick(World* world, vec3 ppos) {
    int pcx = floor_div((int)ppos[0], CHUNK_WIDTH);
    int pcz = floor_div((int)ppos[2], CHUNK_DEPTH);

    int render_dist = WORLD_RENDER_DISTANCE;

    int should_load_count = 0;
    vec2 should_load[256];

    for (int cx = pcx - render_dist; cx <= pcx + render_dist; cx++) {
        for (int cz = pcz - render_dist; cz <= pcz + render_dist; cz++) {
            Chunk* existing = world_get_chunk(world, cx, cz);
            if (!existing) {
                int dx = cx - pcx;
                int dz = cz - pcz;
                if (dx * dx + dz * dz <= render_dist * render_dist) {
                    should_load[should_load_count][0] = (float)cx;
                    should_load[should_load_count][1] = (float)cz;
                    should_load_count++;
                }
            }
        }
    }

    for (int i = 0; i < MAX_LOADED_CHUNKS; i++) {
        if (world->index_map[i] != -1) {
            int cx = (int)world->positions_map[i][0];
            int cz = (int)world->positions_map[i][1];

            int dx = cx - pcx;
            int dz = cz - pcz;

            if (dx * dx + dz * dz > render_dist * render_dist) {
                Chunk* chunk = &world->chunks_map[i];

                if (chunk->data) {
                    free(chunk->data);
                    chunk->data = NULL;
                }
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

                if (chunk->water_model.data) {
                    free(chunk->water_model.data);
                    chunk->water_model.data = NULL;
                }
                if (chunk->water_model.triangles) {
                    free(chunk->water_model.triangles);
                    chunk->water_model.triangles = NULL;
                }
                vbo_free(&chunk->water_model.cache.vbo);
                vao_free(&chunk->water_model.cache.vao);
                ebo_free(&chunk->water_model.cache.ebo);

                world->index_map[i] = -1;
            }
        }
    }

    for (int j = 0; j < should_load_count; j++) {
        int cx = (int)should_load[j][0];
        int cz = (int)should_load[j][1];

        if (!world_get_chunk(world, cx, cz)) {
            for (int i = 0; i < MAX_LOADED_CHUNKS; i++) {
                if (world->index_map[i] == -1) {
                    // Set chunk position for noise-based terrain (noise now fully deterministic)
                    chunk_set_position(cx, cz);
                    
                    Chunk chunk;
                    chunk_generate(&chunk);

                    world->chunks_map[i] = chunk;
                    world->positions_map[i][0] = (float)cx;
                    world->positions_map[i][1] = (float)cz;
                    world->index_map[i] = 1;

world->chunks_map[i].model.pos[0] = (float)cx * CHUNK_WIDTH;
                    world->chunks_map[i].model.pos[1] = 0.0f;
                    world->chunks_map[i].model.pos[2] = (float)cz * CHUNK_DEPTH;

world->chunks_map[i].water_model.pos[0] = (float)cx * CHUNK_WIDTH;
                    world->chunks_map[i].water_model.pos[1] = 0.0f;
                    world->chunks_map[i].water_model.pos[2] = (float)cz * CHUNK_DEPTH;

                    world_queue_rebuild(world, cx, cz);

                    for (int dx = -1; dx <= 1; dx++) {
                        for (int dz = -1; dz <= 1; dz++) {
                            if (dx == 0 && dz == 0) continue;
                            world_queue_rebuild(world, cx + dx, cz + dz);
                        }
                    }
                    break;
                }
            }
        }
    }

    // one chunk per frame at max
    world_process_rebuild_queue(world);
}
