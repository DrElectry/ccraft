#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "core/world.h"
#include "core/world_gen.h"
#include "core/light.h"
#include "utils/rand.h"
#include "utils/log.h"
#include "utils/noise.h"
#include "utils/file.h"
#include "core/game.h"

static void world_install_chunk(World* world, int cx, int cz, uint16_t* data);

void world_init(World* world) {
    world->chunks_map = malloc(sizeof(Chunk) * MAX_LOADED_CHUNKS);
    world->positions_map = malloc(sizeof(vec2) * MAX_LOADED_CHUNKS);
    world->index_map = malloc(sizeof(int) * MAX_LOADED_CHUNKS);

    world->pending_block_capacity = 64000;
    world->pending_block_changes = malloc(sizeof(BlockChange) * world->pending_block_capacity);
    world->pending_block_count = 0;

    noise_set_seed(rng_get_world_seed());

    for (int i = 0; i < MAX_LOADED_CHUNKS; i++) {
        world->index_map[i] = -1;
    }

    world->pending_count = 0;

    world_gen_start();
}

void world_load(World* world, File* file) {
    ASSERT(memcmp(file->data, "CCRAFT", 6) == 0, "%s is not a valid ccraft file", file->src);
    
    int offset = 6;
    
    uint64_t seed;
    memcpy(&seed, file->data + offset, 8);
    offset += 8;
    rng_seed(seed);
    
    float player_data[5];
    memcpy(player_data, file->data + offset, 20);
    offset += 20;
    
    int total_changes;
    memcpy(&total_changes, file->data + offset, sizeof(int));
    offset += sizeof(int);
    
    if (world->pending_block_capacity < total_changes) {
        world->pending_block_capacity = total_changes;
        world->pending_block_changes = realloc(world->pending_block_changes, 
            sizeof(BlockChange) * world->pending_block_capacity);
    }
    
    int changes_size = total_changes * sizeof(BlockChange);
    memcpy(world->pending_block_changes, file->data + offset, changes_size);
    world->pending_block_count = total_changes;
}

void world_save(World* world, const char* filename) {
    File file = file_create(filename);
    
    file_addwrite(&file, "CCRAFT", 6);
    
    uint64_t seed = rng_get_world_seed();
    file_addwrite(&file, (char*)&seed, 8);
    
    float player_data[5] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    file_addwrite(&file, (char*)player_data, 20);
    
    int actual_count = world->pending_block_count;
    file_addwrite(&file, (char*)&actual_count, sizeof(int));
    
    file_addwrite(&file, (char*)world->pending_block_changes, 
                  actual_count * sizeof(BlockChange));
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

static void world_queue_rebuild_3x3(World* world, int cx, int cz);

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

void world_queue_block_change(World* world, int x, int y, int z, uint16_t block) {
    if (world->pending_block_count >= world->pending_block_capacity) {
        world->pending_block_capacity *= 2;
        world->pending_block_changes = realloc(world->pending_block_changes, 
            sizeof(BlockChange) * world->pending_block_capacity);
        if (!world->pending_block_changes) {
            printf("Failed to expand block change buffer\n");
            return;
        }
    }

    for (int i = 0; i < world->pending_block_count; i++) {
        if (world->pending_block_changes[i].x == x && 
            world->pending_block_changes[i].y == y &&
            world->pending_block_changes[i].z == z) {
            world->pending_block_changes[i].block = block;
            return;
        }
    }

    world->pending_block_changes[world->pending_block_count].x = x;
    world->pending_block_changes[world->pending_block_count].y = y;
    world->pending_block_changes[world->pending_block_count].z = z;
    world->pending_block_changes[world->pending_block_count].block = block;
    world->pending_block_count++;
}

int world_set_block(World* world, int wx, int wy, int wz, uint16_t block) {
    if (wy < 0 || wy >= CHUNK_HEIGHT)
        return 0;

    int cx = floor_div(wx, CHUNK_WIDTH);
    int cz = floor_div(wz, CHUNK_DEPTH);

    Chunk* chunk = world_get_chunk(world, cx, cz);
    if (!chunk) {
        world_queue_block_change(world, wx, wy, wz, block);
        return 1;
    }

    int lx = floor_mod(wx, CHUNK_WIDTH);
    int lz = floor_mod(wz, CHUNK_DEPTH);

    int index = lx + CHUNK_WIDTH * (wy + CHUNK_HEIGHT * lz);
    world_queue_block_change(world, wx, wy, wz, block);
    chunk->data[index] = block;

    world_queue_rebuild_3x3(world, cx, cz);

    return 1;
}

void rebuild_chunks_for_block(World* world, int wx, int wy, int wz) {
    (void)wy;
    int cx = floor_div(wx, CHUNK_WIDTH);
    int cz = floor_div(wz, CHUNK_DEPTH);
    world_queue_rebuild_3x3(world, cx, cz);
}

void new_world(const char* filename) {
    File file = file_create(filename);
    
    file_addwrite(&file, "CCRAFT", 6);
    
    uint64_t seed = rng_get_world_seed();
    file_addwrite(&file, (char*)&seed, 8);
    
    float player_data[5] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    file_addwrite(&file, (char*)player_data, 20);
    
    int zero = 0;
    file_addwrite(&file, (char*)&zero, sizeof(int));
}

void world_set_block_at(World* world, vec3 p, uint16_t block) {
    int wx = (int)floorf(p[0]);
    int wy = (int)floorf(p[1]);
    int wz = (int)floorf(p[2]);

    world_set_block(world, wx, wy, wz, block);
}

static void world_invalidate_chunk_mesh(Chunk* chunk) {
    if (chunk)
        chunk->mesh_generation++;
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

    world_invalidate_chunk_mesh(world_get_chunk(world, cx, cz));

    world->pending_rebuilds[world->pending_count][0] = (float)cx;
    world->pending_rebuilds[world->pending_count][1] = (float)cz;
    world->pending_count++;
}

static void world_queue_rebuild_3x3(World* world, int cx, int cz) {
    for (int dx = -1; dx <= 1; dx++) {
        for (int dz = -1; dz <= 1; dz++)
            world_queue_rebuild(world, cx + dx, cz + dz);
    }
}

void world_process_rebuild_queue(World* world) {
    int submitted = 0;

    for (int i = 0; i < world->pending_count && submitted < 8; ) {
        int cx = (int)world->pending_rebuilds[i][0];
        int cz = (int)world->pending_rebuilds[i][1];

        if (!world_get_chunk(world, cx, cz)) {
            for (int j = i; j < world->pending_count - 1; j++) {
                world->pending_rebuilds[j][0] = world->pending_rebuilds[j + 1][0];
                world->pending_rebuilds[j][1] = world->pending_rebuilds[j + 1][1];
            }
            world->pending_count--;
            continue;
        }

        if (world_mesh_in_flight(cx, cz)) {
            i++;
            continue;
        }

        if (!world_mesh_submit(world, cx, cz)) {
            i++;
            continue;
        }

        for (int j = i; j < world->pending_count - 1; j++) {
            world->pending_rebuilds[j][0] = world->pending_rebuilds[j + 1][0];
            world->pending_rebuilds[j][1] = world->pending_rebuilds[j + 1][1];
        }
        world->pending_count--;
        submitted++;
    }
}

void world_rebuild_chunk(World* world, int cx, int cz) {
    world_mesh_submit(world, cx, cz);
}

void world_render(World* world, void* active_program, void* water_program, int cull) {
    vec3 cam_pos;
    glm_vec3_copy(player.camera.pos, cam_pos);

    vec3 forward;
    glm_vec3_copy(player.camera.forward, forward);

    float forward_len2 = forward[0] * forward[0] + forward[1] * forward[1] + forward[2] * forward[2];
    if (forward_len2 > 0.000001f) {
        float inv = 1.0f / sqrtf(forward_len2);
        forward[0] *= inv;
        forward[1] *= inv;
        forward[2] *= inv;
    }

    for (int i = 0; i < MAX_LOADED_CHUNKS; i++) {
        if (world->index_map[i] == -1) continue;

        if (cull) {
            vec3 box_min = {
                world->chunks_map[i].model.pos[0],
                0.0f,
                world->chunks_map[i].model.pos[2]
            };
            vec3 box_max = {
                world->chunks_map[i].model.pos[0] + CHUNK_WIDTH,
                (float)CHUNK_HEIGHT,
                world->chunks_map[i].model.pos[2] + CHUNK_DEPTH
            };

            float max_dot = -1e30f;

            for (int ix = 0; ix < 2; ix++) {
                for (int iy = 0; iy < 2; iy++) {
                    for (int iz = 0; iz < 2; iz++) {
                        vec3 corner = {
                            ix ? box_max[0] : box_min[0],
                            iy ? box_max[1] : box_min[1],
                            iz ? box_max[2] : box_min[2]
                        };

                        float dx = corner[0] - cam_pos[0];
                        float dy = corner[1] - cam_pos[1];
                        float dz = corner[2] - cam_pos[2];
                        float d = forward[0] * dx + forward[1] * dy + forward[2] * dz;
                        if (d > max_dot) max_dot = d;
                    }
                }
            }

            if (max_dot < 0.0f) continue;
        }

        gfx_render(&world->chunks_map[i].model, (Program*)active_program);
        gfx_render(&world->chunks_map[i].water_model, (Program*)water_program);
    }
}

static void world_install_chunk(World* world, int cx, int cz, uint16_t* data) {
    if (world_get_chunk(world, cx, cz)) {
        free(data);
        return;
    }

    for (int i = 0; i < MAX_LOADED_CHUNKS; i++) {
        if (world->index_map[i] != -1)
            continue;

        Chunk* chunk = &world->chunks_map[i];
        chunk->data = data;
        chunk->sky_light = NULL;
        chunk->block_light = NULL;
        chunk->mesh_generation = 0;
        chunk->model = (Render_request){0};
        chunk->water_model = (Render_request){0};

        for (int k = 0; k < world->pending_block_count; k++) {
            int bx = world->pending_block_changes[k].x;
            int by = world->pending_block_changes[k].y;
            int bz = world->pending_block_changes[k].z;

            int chunk_min_x = cx * CHUNK_WIDTH;
            int chunk_max_x = chunk_min_x + CHUNK_WIDTH;
            int chunk_min_z = cz * CHUNK_DEPTH;
            int chunk_max_z = chunk_min_z + CHUNK_DEPTH;

            if (bx >= chunk_min_x && bx < chunk_max_x &&
                bz >= chunk_min_z && bz < chunk_max_z &&
                by >= 0 && by < CHUNK_HEIGHT) {
                int lx = bx - chunk_min_x;
                int lz = bz - chunk_min_z;
                int index = lx + CHUNK_WIDTH * (by + CHUNK_HEIGHT * lz);
                chunk->data[index] = world->pending_block_changes[k].block;
            }
        }

        world->positions_map[i][0] = (float)cx;
        world->positions_map[i][1] = (float)cz;
        world->index_map[i] = 1;

        chunk->model.pos[0] = (float)cx * CHUNK_WIDTH;
        chunk->model.pos[1] = 0.0f;
        chunk->model.pos[2] = (float)cz * CHUNK_DEPTH;

        chunk->water_model.pos[0] = (float)cx * CHUNK_WIDTH;
        chunk->water_model.pos[1] = 0.0f;
        chunk->water_model.pos[2] = (float)cz * CHUNK_DEPTH;

        world_queue_rebuild_3x3(world, cx, cz);
        return;
    }

    free(data);
    printf("Cant add new chunks, world overflow.\n");
}

void world_reload_render_distance(World* world, vec3 ppos) {
    int pcx = floor_div((int)ppos[0], CHUNK_WIDTH);
    int pcz = floor_div((int)ppos[2], CHUNK_DEPTH);

    int render_dist = WORLD_RENDER_DISTANCE;

    for (int cx = pcx - render_dist; cx <= pcx + render_dist; cx++) {
        for (int cz = pcz - render_dist; cz <= pcz + render_dist; cz++) {
            int dx = cx - pcx;
            int dz = cz - pcz;
            if (dx * dx + dz * dz > render_dist * render_dist)
                continue;
            if (!world_get_chunk(world, cx, cz))
                continue;
            world_queue_rebuild_3x3(world, cx, cz);
        }
    }

    world_process_rebuild_queue(world);
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
                chunk_light_free(chunk);
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

    int gen_cx, gen_cz;
    uint16_t* gen_data;
    while (world_gen_poll(&gen_cx, &gen_cz, &gen_data)) {
        int dx = gen_cx - pcx;
        int dz = gen_cz - pcz;
        if (dx * dx + dz * dz > render_dist * render_dist) {
            free(gen_data);
            continue;
        }
        world_install_chunk(world, gen_cx, gen_cz, gen_data);
    }

    int mesh_cx, mesh_cz;
    ChunkMeshResult* mesh;
    uint32_t mesh_gen;
    while (world_mesh_poll(&mesh_cx, &mesh_cz, &mesh, &mesh_gen)) {
        Chunk* chunk = world_get_chunk(world, mesh_cx, mesh_cz);
        if (!chunk || chunk->mesh_generation != mesh_gen) {
            chunk_mesh_result_free(mesh);
            continue;
        }
        chunk_apply_mesh(chunk, mesh);
    }

    for (int j = 0; j < should_load_count; j++) {
        int cx = (int)should_load[j][0];
        int cz = (int)should_load[j][1];

        if (!world_get_chunk(world, cx, cz) && !world_gen_in_flight(cx, cz))
            world_gen_submit(cx, cz);
    }

    world_process_rebuild_queue(world);
}

void world_destroy(World* world) {
    world_gen_stop();

    for (int i = 0; i < MAX_LOADED_CHUNKS; i++) {
        if (world->index_map[i] != -1) {
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
        }
    }

    free(world->chunks_map);
    free(world->positions_map);
    free(world->index_map);
    free(world->pending_block_changes);

    world->chunks_map = NULL;
    world->positions_map = NULL;
    world->index_map = NULL;
    world->pending_block_changes = NULL;
}