#include "chunk.h"
#include <cglm/cglm.h>
#include "tile.h"
#include "gfx.h"
#include "world.h"
#include "rand.h"
#include "noise.h"
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

#define SEA_LEVEL 16
#define BEACH_HEIGHT 2

static int gen_chunk_x = 0;
static int gen_chunk_z = 0;

void chunk_set_position(int cx, int cz) {
    gen_chunk_x = cx;
    gen_chunk_z = cz;
}

int lookup_atlas[] = {
    0,0,0,0,0,0,        // AIR
    1,1,1,1,0,2,        // GRASS
    2,2,2,2,2,2,        // DIRT
    3,3,3,3,3,3,        // LEAVES
    4,4,4,4,4,4,        // STONE
    5,5,5,5,5,5,        // IRON_BLOCK
    16,16,16,16,16,16,  // WATER
    6,6,6,6,7,7,        // LOG
    8,8,8,8,8,8,        // GLASS
    9,9,9,9,9,9,        // COAL_ORE
    10,10,10,10,10,10,  // COPPER_ORE
    11,11,11,11,11,11,  // GOLD_ORE
    12,12,12,12,12,12,  // SAND
};

int lookup_transparent[] = {
    1, // AIR
    0, // GRASS
    0, // DIRT
    1, // LEAVES
    0, // STONE
    0, // IRON_BLOCK
    0, // WATER
    0, // LOG
    1, // GLASS
    0, // COAL_ORE
    0, // COPPER_ORE
    0, // GOLD_ORE
    0, // SAND
};

inline int atlas_lookup(uint16_t tile_id, enum Tile_face face)
{
    return lookup_atlas[tile_id * 6 + face];
}

static int get_terrain_height(int wx, int wz) {
    float h = fbm2d(wx*0.025, wz*0.025, 4, 0.25, 0.5)*16.0;
    float h2 = fbm2d(wx*0.05-100.0, wz*0.05-100.0, 2, 0.25, 1.5);
    if (h2 < h) {h-=h2*12.0;}
    return h+20.0f;
}

static uint16_t get_block_at_height(int wy, int wx, int wz, int terrain_height, int water_level) {
    int distance_to_water = terrain_height - water_level;
    
    if (wy <= water_level && wy > terrain_height) {
        return WATER;
    }
    
    if (wy > terrain_height) {
        return AIR;
    }

    int is_surface = (wy == terrain_height);
    
    if (is_surface) {
        if (terrain_height <= water_level) {
            return SAND;
        }
        
        if (terrain_height <= water_level + BEACH_HEIGHT) {
            float blend = (terrain_height - water_level) / (float)BEACH_HEIGHT;
            float random_factor = RANDF();
            
            if (random_factor < blend) {
                return GRASS;
            } else {
                return SAND;
            }
        }
        
        return GRASS;
    }

    if (terrain_height <= water_level + BEACH_HEIGHT) {
        if (wy >= terrain_height - 2) {
            return SAND;
        } else if (wy >= terrain_height - 4) {
            return DIRT;
        } else {
            return STONE;
        }
    }

    if (terrain_height > water_level + BEACH_HEIGHT) {
        if (wy == terrain_height) {
            return GRASS;
        } else if (wy >= terrain_height - 3) {
            return DIRT;
        } else {
            return STONE;
        }
    } else if (wy >= terrain_height - 2) {
        if (wy == terrain_height) {
            return GRASS;
        }
        return DIRT;
    } else {
        if (wy == terrain_height) {
            return DIRT;
        } else if (wy >= terrain_height - 4) {
            return DIRT;
        } else {
            return STONE;
        }
    }
}

void chunk_generate(Chunk* chunk) {
    // Seed per-chunk RNG for deterministic features (ores, trees)
    rng_seed_chunk(gen_chunk_x, gen_chunk_z);
    
    chunk->data = (uint16_t*)malloc(CHUNK_WIDTH * CHUNK_HEIGHT * CHUNK_DEPTH * sizeof(uint16_t));

    for (int x = 0; x < CHUNK_WIDTH; x++) {
        for (int z = 0; z < CHUNK_DEPTH; z++) {

            int wx = gen_chunk_x * CHUNK_WIDTH + x;
            int wz = gen_chunk_z * CHUNK_DEPTH + z;

            int terrain_height = get_terrain_height(wx, wz);

            for (int y = 0; y < CHUNK_HEIGHT; y++) {
                int index = x + CHUNK_WIDTH * (y + CHUNK_HEIGHT * z);
                uint16_t block = get_block_at_height(y, wx, wz, terrain_height, SEA_LEVEL);
                chunk->data[index] = block;
            }
        }
    }

    for (int x = 2; x < CHUNK_WIDTH - 2; x += 5) {
        for (int z = 2; z < CHUNK_DEPTH - 2; z += 5) {

            int wx = gen_chunk_x * CHUNK_WIDTH + x;
            int wz = gen_chunk_z * CHUNK_DEPTH + z;

            float density = fbm2d(wx * 0.08f, wz * 0.08f, 2, 0.5f, 2.0f);
            if (density < 0.4f) continue;

            int terrain_height = get_terrain_height(wx, wz);
            if (terrain_height <= SEA_LEVEL + 6) continue;

            int surface_y = terrain_height;
            int surface_idx = x + CHUNK_WIDTH * (surface_y + CHUNK_HEIGHT * z);

            if (surface_idx < 0 || surface_idx >= CHUNK_WIDTH * CHUNK_HEIGHT * CHUNK_DEPTH)
                continue;

            if (chunk->data[surface_idx] != GRASS)
                continue;

            int trunk_h = 3 + RAND(0, 3);

            for (int th = 1; th <= trunk_h; th++) {
                int ty = surface_y + th;
                if (ty >= CHUNK_HEIGHT) break;

                int idx = x + CHUNK_WIDTH * (ty + CHUNK_HEIGHT * z);
                chunk->data[idx] = LOG;
            }

            int canopy_base = surface_y + trunk_h;

            for (int ly = 0; ly < 2; ly++) {
                for (int dx = -2; dx <= 2; dx++) {
                    for (int dz = -2; dz <= 2; dz++) {

                        int lx = x + dx;
                        int lz = z + dz;
                        int ly_ = canopy_base + ly;

                        if (lx < 0 || lx >= CHUNK_WIDTH ||
                            lz < 0 || lz >= CHUNK_DEPTH ||
                            ly_ >= CHUNK_HEIGHT)
                            continue;

                        int idx = lx + CHUNK_WIDTH * (ly_ + CHUNK_HEIGHT * lz);
                        chunk->data[idx] = LEAVES;
                    }
                }
            }

            for (int ly = 2; ly < 4; ly++) {

                int lx = x;
                int lz = z;
                int ly_ = canopy_base + ly;

                if (ly_ < CHUNK_HEIGHT) {
                    int idx = lx + CHUNK_WIDTH * (ly_ + CHUNK_HEIGHT * lz);
                    chunk->data[idx] = LEAVES;
                }

                int dirs[4][2] = {{0,1},{0,-1},{1,0},{-1,0}};

                for (int d = 0; d < 4; d++) {
                    int dx = dirs[d][0];
                    int dz = dirs[d][1];

                    int lx2 = x + dx;
                    int lz2 = z + dz;

                    if (lx2 < 0 || lx2 >= CHUNK_WIDTH ||
                        lz2 < 0 || lz2 >= CHUNK_DEPTH ||
                        ly_ >= CHUNK_HEIGHT)
                        continue;

                    int idx = lx2 + CHUNK_WIDTH * (ly_ + CHUNK_HEIGHT * lz2);
                    chunk->data[idx] = LEAVES;
                }
            }
        }
    }

    for (int i = 0; i < 10; i++) { // ores
        {
            int x = RAND(0, CHUNK_WIDTH - 1);
            int y = RAND(0, 32);
            int z = RAND(0, CHUNK_DEPTH - 1);

            if (chunk->data[x + CHUNK_WIDTH * (y + CHUNK_HEIGHT * z)] != STONE)
                goto coal_skip; // goto yuck, but its required

            int len = 20 + RAND(0, 25);

            float dx = (RANDF() - 0.5f) * 2.0f;
            float dy = (RANDF() - 0.5f) * 1.0f;
            float dz = (RANDF() - 0.5f) * 2.0f;

            for (int s = 0; s < len; s++) {

                int rx = x + RAND(-1, 1);
                int ry = y + RAND(-1, 1);
                int rz = z + RAND(-1, 1);

                if (rx < 0 || rx >= CHUNK_WIDTH ||
                    ry < 0 || ry >= CHUNK_HEIGHT ||
                    rz < 0 || rz >= CHUNK_DEPTH)
                    continue;

                int idx = rx + CHUNK_WIDTH * (ry + CHUNK_HEIGHT * rz);

                if (chunk->data[idx] == STONE)
                    chunk->data[idx] = COAL_ORE;

                x += dx;
                y += dy;
                z += dz;
            }

            coal_skip:;
        }

        {
            int x = RAND(0, CHUNK_WIDTH - 1);
            int y = RAND(0, 28);
            int z = RAND(0, CHUNK_DEPTH - 1);

            if (chunk->data[x + CHUNK_WIDTH * (y + CHUNK_HEIGHT * z)] != STONE)
                goto copper_skip;

            int len = 15 + RAND(0, 20);

            float dx = (RANDF() - 0.5f) * 2.0f;
            float dy = (RANDF() - 0.5f) * 1.0f;
            float dz = (RANDF() - 0.5f) * 2.0f;

            for (int s = 0; s < len; s++) {

                int rx = x + RAND(-1, 1);
                int ry = y + RAND(-1, 1);
                int rz = z + RAND(-1, 1);

                if (rx < 0 || rx >= CHUNK_WIDTH ||
                    ry < 0 || ry >= CHUNK_HEIGHT ||
                    rz < 0 || rz >= CHUNK_DEPTH)
                    continue;

                int idx = rx + CHUNK_WIDTH * (ry + CHUNK_HEIGHT * rz);

                if (chunk->data[idx] == STONE)
                    chunk->data[idx] = COPPER_ORE;

                x += dx;
                y += dy;
                z += dz;
            }

            copper_skip:;
        }

        {
            int x = RAND(0, CHUNK_WIDTH - 1);
            int y = RAND(0, 24);
            int z = RAND(0, CHUNK_DEPTH - 1);

            if (chunk->data[x + CHUNK_WIDTH * (y + CHUNK_HEIGHT * z)] != STONE)
                goto gold_skip;

            int len = 10 + RAND(0, 15);

            float dx = (RANDF() - 0.5f) * 1.5f;
            float dy = (RANDF() - 0.5f) * 1.0f;
            float dz = (RANDF() - 0.5f) * 1.5f;

            for (int s = 0; s < len; s++) {

                int rx = x + RAND(-1, 1);
                int ry = y + RAND(-1, 1);
                int rz = z + RAND(-1, 1);

                if (rx < 0 || rx >= CHUNK_WIDTH ||
                    ry < 0 || ry >= CHUNK_HEIGHT ||
                    rz < 0 || rz >= CHUNK_DEPTH)
                    continue;

                int idx = rx + CHUNK_WIDTH * (ry + CHUNK_HEIGHT * rz);

                if (chunk->data[idx] == STONE)
                    chunk->data[idx] = GOLD_ORE;

                x += dx;
                y += dy;
                z += dz;
            }

            gold_skip:;
        }
    }

    chunk->model = (Render_request){0};
    chunk->water_model = (Render_request){0};
}

static inline int is_opaque_block(uint16_t tile_id) {
    return tile_id != AIR && tile_id != LEAVES && tile_id != WATER;
}

static inline int is_water_block(uint16_t tile_id) {
    return tile_id == WATER;
}

static void free_render_request(Render_request* r) {
    if (r->data) {
        free(r->data);
        r->data = NULL;
    }
    if (r->triangles) {
        free(r->triangles);
        r->triangles = NULL;
    }
    vbo_free(&r->cache.vbo);
    vao_free(&r->cache.vao);
    ebo_free(&r->cache.ebo);
}

static void finalize_render_request(Render_request* r, float* vertices, int* indices, int v_cursor, int i_cursor) {
    if (v_cursor > 0) {
        vertices = (float*)realloc(vertices, v_cursor * sizeof(float));
        indices = (int*)realloc(indices, i_cursor * sizeof(int));

        r->data = vertices;
        r->triangles = indices;
        r->data_size = v_cursor * sizeof(float);
        r->tri_count = i_cursor / 3;

        glm_vec3_copy((vec3){0.0f,0.0f,0.0f}, r->rot);
        glm_vec3_copy((vec3){1.0f,1.0f,1.0f}, r->scale);

        gfx_packet_static_request(r);
    } else {
        free(vertices);
        free(indices);

        r->data = NULL;
        r->triangles = NULL;
        r->data_size = 0;
        r->tri_count = 0;
    }
}

static void push_face_to_buffer(uint16_t tile_id, float* pos, World* world, int wx, int wy, int wz,
                                  float* model_verts, int* model_inds, int* v_cur, int* i_cur,
                                  float* water_verts, int* water_inds, int* w_v_cur, int* w_i_cur) {
    
    uint16_t front = world_get_block(world, wx, wy, wz + 1);
    uint16_t back  = world_get_block(world, wx, wy, wz - 1);
    uint16_t left  = world_get_block(world, wx - 1, wy, wz);
    uint16_t right = world_get_block(world, wx + 1, wy, wz);
    uint16_t up    = world_get_block(world, wx, wy + 1, wz);
    uint16_t down  = world_get_block(world, wx, wy - 1, wz);

    int render_front;
    if (is_water_block(tile_id)) {
        // water culls by water
        render_front = !is_water_block(front);
    } else {
        // non water blocks are never culled by water
        render_front = lookup_transparent[front] || (is_water_block(front) ? 1 : 0);
    }
    if (render_front) {
        if (is_water_block(tile_id)) {
            tile_push_face(water_verts, (unsigned int*)water_inds, pos, w_v_cur, w_i_cur, FRONT, atlas_lookup(tile_id, FRONT));
        } else {
            tile_push_face(model_verts, (unsigned int*)model_inds, pos, v_cur, i_cur, FRONT, atlas_lookup(tile_id, FRONT));
        }
    }

    int render_back;
    if (is_water_block(tile_id)) {
        render_back = !is_water_block(back);
    } else {
        render_back = lookup_transparent[back] || (is_water_block(back) ? 1 : 0);
    }
    if (render_back) {
        if (is_water_block(tile_id)) {
            tile_push_face(water_verts, (unsigned int*)water_inds, pos, w_v_cur, w_i_cur, BACK, atlas_lookup(tile_id, BACK));
        } else {
            tile_push_face(model_verts, (unsigned int*)model_inds, pos, v_cur, i_cur, BACK, atlas_lookup(tile_id, BACK));
        }
    }

    int render_left;
    if (is_water_block(tile_id)) {
        render_left = !is_water_block(left);
    } else {
        render_left = lookup_transparent[left] || (is_water_block(left) ? 1 : 0);
    }
    if (render_left) {
        if (is_water_block(tile_id)) {
            tile_push_face(water_verts, (unsigned int*)water_inds, pos, w_v_cur, w_i_cur, LEFT, atlas_lookup(tile_id, LEFT));
        } else {
            tile_push_face(model_verts, (unsigned int*)model_inds, pos, v_cur, i_cur, LEFT, atlas_lookup(tile_id, LEFT));
        }
    }

    int render_right;
    if (is_water_block(tile_id)) {
        render_right = !is_water_block(right);
    } else {
        render_right = lookup_transparent[right] || (is_water_block(right) ? 1 : 0);
    }
    if (render_right) {
        if (is_water_block(tile_id)) {
            tile_push_face(water_verts, (unsigned int*)water_inds, pos, w_v_cur, w_i_cur, RIGHT, atlas_lookup(tile_id, RIGHT));
        } else {
            tile_push_face(model_verts, (unsigned int*)model_inds, pos, v_cur, i_cur, RIGHT, atlas_lookup(tile_id, RIGHT));
        }
    }

    int render_up;
    if (is_water_block(tile_id)) {
        render_up = !is_water_block(up);
    } else {
        render_up = lookup_transparent[up] || (is_water_block(up) ? 1 : 0);
    }
    if (render_up) {
        if (is_water_block(tile_id)) {
            tile_push_face(water_verts, (unsigned int*)water_inds, pos, w_v_cur, w_i_cur, UP, atlas_lookup(tile_id, UP));
        } else {
            tile_push_face(model_verts, (unsigned int*)model_inds, pos, v_cur, i_cur, UP, atlas_lookup(tile_id, UP));
        }
    }

    int render_down;
    if (is_water_block(tile_id)) {
        render_down = !is_water_block(down);
    } else {
        render_down = lookup_transparent[down] || (is_water_block(down) ? 1 : 0);
    }
    if (render_down) {
        if (is_water_block(tile_id)) {
            tile_push_face(water_verts, (unsigned int*)water_inds, pos, w_v_cur, w_i_cur, DOWN, atlas_lookup(tile_id, DOWN));
        } else {
            tile_push_face(model_verts, (unsigned int*)model_inds, pos, v_cur, i_cur, DOWN, atlas_lookup(tile_id, DOWN));
        }
    }
}

void chunk_rebuild(Chunk* chunk, struct World* world, int cx, int cz) {
    free_render_request(&chunk->model);
    free_render_request(&chunk->water_model);

    int max_blocks = CHUNK_WIDTH * CHUNK_HEIGHT * CHUNK_DEPTH;
    int max_faces = max_blocks * 6;
    int max_vertices = max_faces * 4;
    int max_indices = max_faces * 6;

    float* model_vertices = (float*)malloc(max_vertices * 5 * sizeof(float));
    int* model_indices = (int*)malloc(max_indices * sizeof(int));
    float* water_vertices = (float*)malloc(max_vertices * 5 * sizeof(float));
    int* water_indices = (int*)malloc(max_indices * sizeof(int));

    int v_cursor = 0, i_cursor = 0;
    int w_v_cursor = 0, w_i_cursor = 0;

    int wx_offset = cx * CHUNK_WIDTH;
    int wz_offset = cz * CHUNK_DEPTH;

    for (int x = 0; x < CHUNK_WIDTH; x++) {
        for (int y = 0; y < CHUNK_HEIGHT; y++) {
            for (int z = 0; z < CHUNK_DEPTH; z++) {
                int index = x + CHUNK_WIDTH * (y + CHUNK_HEIGHT * z);
                uint16_t tile_id = chunk->data[index];

                if (tile_id == AIR)
                    continue;

                int wx = wx_offset + x;
                int wy = y;
                int wz = wz_offset + z;

                float pos[3] = {(float)x, (float)y, (float)z};

                push_face_to_buffer(tile_id, pos, world, wx, wy, wz,
                                  model_vertices, model_indices, &v_cursor, &i_cursor,
                                  water_vertices, water_indices, &w_v_cursor, &w_i_cursor);
            }
        }
    }

    finalize_render_request(&chunk->model, model_vertices, model_indices, v_cursor, i_cursor);
    finalize_render_request(&chunk->water_model, water_vertices, water_indices, w_v_cursor, w_i_cursor);
}