#include "core/chunk.h"
#include "core/light.h"
#include <cglm/cglm.h>
#include "core/tile.h"
#include "core/gfx.h"
#include "core/world.h"
#include "utils/rand.h"
#include "utils/noise.h"
#include "core/game.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define SEA_LEVEL 24
#define BEACH_HEIGHT 4
#define SUPERFLAT_DEBUG 0

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
    13,13,13,13,13,13,  // GRAVEL
    18,18,18,18,18,18,  // LAVA
    32,32,32,32,32,32,  // ROSE
    33,33,33,33,33,33,  // GRASS_CROSS
    14,14,14,14,14,14,  // BORDER
    15,15,15,15,15,15,  // PLANKS
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
    0, // GRAVEL
    0, // LAVA
    1, // ROSE
    1, // GRASS_CROSS
    0, // BORDER
    0, // PLANKS
};

int lookup_sounds[] = {
    0, // AIR
    0, // GRASS
    2, // DIRT
    0, // LEAVES
    1, // STONE
    1, // IRON_BLOCK
    0, // WATER
    3, // LOG
    1, // GLASS
    1, // COAL_ORE
    1, // COPPER_ORE
    1, // GOLD_ORE
    2, // SAND
    2, // GRAVEL
    0, // LAVA
    0, // ROSE
    0, // GRASS_CROSS
    1, // BORDER
    3, // PLANKS
};

int lookup_cross[] = {
    0, // AIR
    0, // GRASS
    0, // DIRT
    0, // LEAVES
    0, // STONE
    0, // IRON_BLOCK
    0, // WATER
    0, // LOG
    0, // GLASS
    0, // COAL_ORE
    0, // COPPER_ORE
    0, // GOLD_ORE
    0, // SAND
    0, // GRAVEL
    0, // LAVA
    1, // ROSE
    1, // GRASS_CROSS
    0, // BORDER
    0, // PLANKS
};

int lookup_ignorecollision[] = {
    0, // AIR
    0, // GRASS
    0, // DIRT
    0, // LEAVES
    0, // STONE
    0, // IRON_BLOCK
    1, // WATER
    0, // LOG
    0, // GLASS
    0, // COAL_ORE
    0, // COPPER_ORE
    0, // GOLD_ORE
    0, // SAND
    0, // GRAVEL
    1, // LAVA
    1, // ROSE
    1, // GRASS_CROSS
    0, // BORDER
    0, // PLANKS
};

static inline int is_cross_block(uint16_t tile_id) {
    if (tile_id >= (uint16_t)(sizeof(lookup_cross) / sizeof(lookup_cross[0])))
        return 0;
    return lookup_cross[tile_id] != 0;
}


uint16_t chunk_sound_pack(uint16_t tile_id)
{
    if (tile_id >= (uint16_t)(sizeof(lookup_sounds) / sizeof(lookup_sounds[0])))
        return 0;
    return (uint16_t)lookup_sounds[tile_id];
}

inline int atlas_lookup(uint16_t tile_id, enum Tile_face face)
{
    return lookup_atlas[tile_id * 6 + face];
}

static int get_terrain_height(int wx, int wz) {
    float h = fbm2d(wx*0.015, wz*0.015, 4, 0.5, 0.5)*24.0;
    float h2 = fbm2d(wx*0.05-100.0, wz*0.05-100.0, 2, 0.5, 1.5);
    float h3 = noise2d(wx*0.045, wz*0.045)*10.0;
    if (h2 < h-h3*0.3) {h-=16.0f-h2*8.0;}
    return h3+h+32.0f;
}

static uint16_t get_block_at_height(int wy, int wx, int wz, int terrain_height, int water_level) {
    int distance_to_water = terrain_height - water_level;

    if (wy == 0) {
        return BORDER;
    }
    
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
            if (RANDF() < 0.0035f) {
                return LAVA;
            }
            return SAND;
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

void chunk_generate(Chunk* chunk, int cx, int cz) {
    RNG rng;
    rng_seed_chunk_r(&rng, cx, cz);

    #undef RAND
    #undef RANDF
    #define RAND(min, max) rng_int_r(&rng, (min), (max))
    #define RANDF() rng_float_r(&rng) // as our chunks start to generate in parallel, we have to do this undef

    chunk->data = (uint16_t*)malloc(CHUNK_WIDTH * CHUNK_HEIGHT * CHUNK_DEPTH * sizeof(uint16_t));
    chunk->sky_light = NULL;
    chunk->block_light = NULL;

    #if SUPERFLAT_DEBUG

        for (int x = 0; x < CHUNK_WIDTH; x++) {
            for (int z = 0; z < CHUNK_DEPTH; z++) {
                for (int y = 0; y < CHUNK_HEIGHT; y++) {

                    int idx = x + CHUNK_WIDTH * (y + CHUNK_HEIGHT * z);

                    if (y == 0)
                        chunk->data[idx] = BORDER;
                    else if (y <= 2)
                        chunk->data[idx] = DIRT;
                    else if (y == 3)
                        chunk->data[idx] = GRASS;
                    else
                        chunk->data[idx] = AIR;
                }
            }
        }

        chunk->model = (Render_request){0};
        chunk->water_model = (Render_request){0};

        #undef RAND
        #undef RANDF
        #define RAND(min, max) rng_int((min), (max))
        #define RANDF() rng_float()

        return;

    #endif

    for (int x = 0; x < CHUNK_WIDTH; x++) {
        for (int z = 0; z < CHUNK_DEPTH; z++) {

            int wx = cx * CHUNK_WIDTH + x;
            int wz = cz * CHUNK_DEPTH + z;

            int terrain_height = get_terrain_height(wx, wz);

            for (int y = 0; y < CHUNK_HEIGHT; y++) {
                int index = x + CHUNK_WIDTH * (y + CHUNK_HEIGHT * z);
                uint16_t block = get_block_at_height(y, wx, wz, terrain_height, SEA_LEVEL);

                chunk->data[index] = block;

            }
        }
    }

    for (int x = 0; x < CHUNK_WIDTH; x++) {
        for (int z = 0; z < CHUNK_DEPTH; z++) {
            int wx = cx * CHUNK_WIDTH + x;
            int wz = cz * CHUNK_DEPTH + z;
            int terrain_height = get_terrain_height(wx, wz);
            
            if (terrain_height <= SEA_LEVEL + BEACH_HEIGHT + 1) continue;
            
            int surface_y = terrain_height;
            int idx = x + CHUNK_WIDTH * (surface_y + CHUNK_HEIGHT * z);
            if (chunk->data[idx] != GRASS) continue;
            
            if (RANDF() > 0.25f) continue;
            
            uint16_t decoration = (RANDF() < 0.9f) ? GRASS_CROSS : ROSE;
            
            int above_y = surface_y + 1;
            if (above_y < CHUNK_HEIGHT) {
                int above_idx = x + CHUNK_WIDTH * (above_y + CHUNK_HEIGHT * z);
                chunk->data[above_idx] = decoration;
            }
        }
    }

    for (int x = 2; x < CHUNK_WIDTH - 2; x += 5) {
        for (int z = 2; z < CHUNK_DEPTH - 2; z += 5) {

            int wx = cx * CHUNK_WIDTH + x;
            int wz = cz * CHUNK_DEPTH + z;

            float density = fbm2d(wx * 0.08f, wz * 0.08f, 2, 0.5f, 2.0f);
            if (density < 0.25f) continue;

            int terrain_height = get_terrain_height(wx, wz);
            if (terrain_height <= SEA_LEVEL + 6) continue;

            int surface_y = terrain_height;
            int surface_idx = x + CHUNK_WIDTH * (surface_y + CHUNK_HEIGHT * z);

            if (surface_idx < 0 || surface_idx >= CHUNK_WIDTH * CHUNK_HEIGHT * CHUNK_DEPTH)
                continue;

            if (chunk->data[surface_idx] != GRASS)
                continue;

            int trunk_h = 4 + RAND(0, 3);

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
            for (int th = 1; th <= trunk_h; th++) {
                int ty = surface_y + th;
                if (ty >= CHUNK_HEIGHT) break;

                int idx = x + CHUNK_WIDTH * (ty + CHUNK_HEIGHT * z);
                chunk->data[idx] = LOG;
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
                    chunk->data[idx] = IRON_ORE;

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

    // restore their original forms

    #undef RAND
    #undef RANDF
    #define RAND(min, max) rng_int((min), (max))
    #define RANDF() rng_float()
}


static inline int is_opaque_block(uint16_t tile_id) {
    return tile_id != AIR && tile_id != LEAVES && tile_id != WATER && tile_id != LAVA;
}

static inline int is_liquid_block(uint16_t tile_id) {
    return tile_id == WATER || tile_id == LAVA;
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

static void assign_render_request(Render_request* r, float* vertices, int* indices, int v_cursor, int i_cursor) {
    if (v_cursor > 0) {
        vertices = (float*)realloc(vertices, v_cursor * sizeof(float));
        indices = (int*)realloc(indices, i_cursor * sizeof(int));

        r->data = vertices;
        r->triangles = indices;
        r->data_size = v_cursor * sizeof(float);
        r->tri_count = i_cursor / 3;

        glm_vec3_copy((vec3){0.0f,0.0f,0.0f}, r->rot);
        glm_vec3_copy((vec3){1.0f,1.0f,1.0f}, r->scale);
    } else {
        free(vertices);
        free(indices);

        r->data = NULL;
        r->triangles = NULL;
        r->data_size = 0;
        r->tri_count = 0;
    }
}

static inline int mesh_floor_div(int a, int b) {
    if (a >= 0) return a / b;
    return - ((-a + b - 1) / b);
}

static inline int mesh_floor_mod(int a, int b) {
    int r = a % b;
    if (r < 0) r += b;
    return r;
}

void chunk_neighbors_capture(ChunkNeighbors* n, struct World* world, int cx, int cz) {
    n->cx = cx;
    n->cz = cz;

    for (int dx = -1; dx <= 1; dx++) {
        for (int dz = -1; dz <= 1; dz++) {
            int gx = dx + 1;
            int gz = dz + 1;
            Chunk* ch = world_get_chunk(world, cx + dx, cz + dz);
            if (ch && ch->data) {
                memcpy(n->chunks[gx][gz], ch->data, CHUNK_BLOCK_COUNT * sizeof(uint16_t));
                n->loaded[gx][gz] = 1;
            } else {
                memset(n->chunks[gx][gz], 0, CHUNK_BLOCK_COUNT * sizeof(uint16_t));
                n->loaded[gx][gz] = 0;
            }
        }
    }
}

uint16_t chunk_neighbors_get(const ChunkNeighbors* n, int wx, int wy, int wz) {
    if (wy < 0 || wy >= CHUNK_HEIGHT)
        return AIR;

    int cx = mesh_floor_div(wx, CHUNK_WIDTH);
    int cz = mesh_floor_div(wz, CHUNK_DEPTH);
    int gx = cx - (n->cx - 1);
    int gz = cz - (n->cz - 1);

    if (gx < 0 || gx > 2 || gz < 0 || gz > 2 || !n->loaded[gx][gz])
        return AIR;

    int lx = mesh_floor_mod(wx, CHUNK_WIDTH);
    int lz = mesh_floor_mod(wz, CHUNK_DEPTH);
    int index = lx + CHUNK_WIDTH * (wy + CHUNK_HEIGHT * lz);
    return n->chunks[gx][gz][index];
}

void chunk_mesh_result_free(ChunkMeshResult* mesh) {
    if (!mesh)
        return;
    free(mesh->model_verts);
    free(mesh->model_inds);
    free(mesh->water_verts);
    free(mesh->water_inds);
    free(mesh->sky_light);
    free(mesh->block_light);
    free(mesh);
}

void chunk_apply_mesh(Chunk* chunk, ChunkMeshResult* mesh) {
    if (mesh->sky_light && mesh->block_light) {
        chunk_light_alloc(chunk);
        if (chunk->sky_light && chunk->block_light) {
            memcpy(chunk->sky_light, mesh->sky_light, CHUNK_BLOCK_COUNT);
            memcpy(chunk->block_light, mesh->block_light, CHUNK_BLOCK_COUNT);
        }
    }

    free_render_request(&chunk->model);
    free_render_request(&chunk->water_model);

    assign_render_request(&chunk->model, mesh->model_verts, mesh->model_inds,
                          mesh->model_v, mesh->model_i);
    assign_render_request(&chunk->water_model, mesh->water_verts, mesh->water_inds,
                          mesh->water_v, mesh->water_i);

    if (chunk->model.data)
        gfx_chunk_packet_static_request(&chunk->model);
    if (chunk->water_model.data)
        gfx_chunk_packet_static_request(&chunk->water_model);

    free(mesh);
}

static void push_face_to_buffer(uint16_t tile_id, float* pos,
                                  const uint8_t* sky_light, const uint8_t* block_light,
                                  int lx, int ly, int lz,
                                  const ChunkNeighbors* n, int wx, int wy, int wz,
                                  float* model_verts, int* model_inds, int* v_cur, int* i_cur,
                                  float* water_verts, int* water_inds, int* w_v_cur, int* w_i_cur) {
    (void)tile_id;
    (void)pos;
    (void)sky_light;
    (void)block_light;
    (void)lx;
    (void)ly;
    (void)lz;
    (void)n;
    (void)wx;
    (void)wy;
    (void)wz;
    (void)model_verts;
    (void)model_inds;
    (void)v_cur;
    (void)i_cur;
    (void)water_verts;
    (void)water_inds;
    (void)w_v_cur;
    (void)w_i_cur;

    uint16_t front = chunk_neighbors_get(n, wx, wy, wz + 1);
    uint16_t back  = chunk_neighbors_get(n, wx, wy, wz - 1);
    uint16_t left  = chunk_neighbors_get(n, wx - 1, wy, wz);
    uint16_t right = chunk_neighbors_get(n, wx + 1, wy, wz);
    uint16_t up    = chunk_neighbors_get(n, wx, wy + 1, wz);
    uint16_t down  = chunk_neighbors_get(n, wx, wy - 1, wz);


    int render_front;
    if (is_liquid_block(tile_id)) {
        // water culls by water
        render_front = !is_liquid_block(front);
    } else {
        // non water blocks are never culled by water
        render_front = lookup_transparent[front] || (is_liquid_block(front) ? 1 : 0);
    }
    if (render_front) {
        float lf = chunk_light_face_vertex(sky_light, block_light, lx, ly, lz, FRONT);
        if (is_liquid_block(tile_id)) {
            tile_push_face(water_verts, (unsigned int*)water_inds, pos, w_v_cur, w_i_cur, FRONT, atlas_lookup(tile_id, FRONT), lf);
        } else {
            tile_push_face(model_verts, (unsigned int*)model_inds, pos, v_cur, i_cur, FRONT, atlas_lookup(tile_id, FRONT), lf);
        }
    }

    int render_back;
    if (is_liquid_block(tile_id)) {
        render_back = !is_liquid_block(back);
    } else {
        render_back = lookup_transparent[back] || (is_liquid_block(back) ? 1 : 0);
    }
    if (render_back) {
        float lf = chunk_light_face_vertex(sky_light, block_light, lx, ly, lz, BACK);
        if (is_liquid_block(tile_id)) {
            tile_push_face(water_verts, (unsigned int*)water_inds, pos, w_v_cur, w_i_cur, BACK, atlas_lookup(tile_id, BACK), lf);
        } else {
            tile_push_face(model_verts, (unsigned int*)model_inds, pos, v_cur, i_cur, BACK, atlas_lookup(tile_id, BACK), lf);
        }
    }

    int render_left;
    if (is_liquid_block(tile_id)) {
        render_left = !is_liquid_block(left);
    } else {
        render_left = lookup_transparent[left] || (is_liquid_block(left) ? 1 : 0);
    }
    if (render_left) {
        float lf = chunk_light_face_vertex(sky_light, block_light, lx, ly, lz, LEFT);
        if (is_liquid_block(tile_id)) {
            tile_push_face(water_verts, (unsigned int*)water_inds, pos, w_v_cur, w_i_cur, LEFT, atlas_lookup(tile_id, LEFT), lf);
        } else {
            tile_push_face(model_verts, (unsigned int*)model_inds, pos, v_cur, i_cur, LEFT, atlas_lookup(tile_id, LEFT), lf);
        }
    }

    int render_right;
    if (is_liquid_block(tile_id)) {
        render_right = !is_liquid_block(right);
    } else {
        render_right = lookup_transparent[right] || (is_liquid_block(right) ? 1 : 0);
    }
    if (render_right) {
        float lf = chunk_light_face_vertex(sky_light, block_light, lx, ly, lz, RIGHT);
        if (is_liquid_block(tile_id)) {
            tile_push_face(water_verts, (unsigned int*)water_inds, pos, w_v_cur, w_i_cur, RIGHT, atlas_lookup(tile_id, RIGHT), lf);
        } else {
            tile_push_face(model_verts, (unsigned int*)model_inds, pos, v_cur, i_cur, RIGHT, atlas_lookup(tile_id, RIGHT), lf);
        }
    }

    int render_up;
    if (is_liquid_block(tile_id)) {
        render_up = !is_liquid_block(up);
    } else {
        render_up = lookup_transparent[up] || (is_liquid_block(up) ? 1 : 0);
    }
    if (render_up) {
        float lf = chunk_light_face_vertex(sky_light, block_light, lx, ly, lz, UP);
        if (is_liquid_block(tile_id)) {
            tile_push_face(water_verts, (unsigned int*)water_inds, pos, w_v_cur, w_i_cur, UP, atlas_lookup(tile_id, UP), lf);
        } else {
            tile_push_face(model_verts, (unsigned int*)model_inds, pos, v_cur, i_cur, UP, atlas_lookup(tile_id, UP), lf);
        }
    }

    int render_down;
    if (is_liquid_block(tile_id)) {
        render_down = !is_liquid_block(down);
    } else {
        render_down = lookup_transparent[down] || (is_liquid_block(down) ? 1 : 0);
    }
    if (render_down) {
        float lf = chunk_light_face_vertex(sky_light, block_light, lx, ly, lz, DOWN);
        if (is_liquid_block(tile_id)) {
            tile_push_face(water_verts, (unsigned int*)water_inds, pos, w_v_cur, w_i_cur, DOWN, atlas_lookup(tile_id, DOWN), lf);
        } else {
            tile_push_face(model_verts, (unsigned int*)model_inds, pos, v_cur, i_cur, DOWN, atlas_lookup(tile_id, DOWN), lf);
        }
    }
}

void chunk_build_mesh(ChunkMeshResult* out, const uint16_t* data,
                      const uint8_t* sky_light, const uint8_t* block_light,
                      const ChunkNeighbors* n, int cx, int cz) {
    int max_blocks = CHUNK_BLOCK_COUNT;
    int max_faces = max_blocks * 6;
    int max_vertices = max_faces * 4;
    int max_indices = max_faces * 6;

    float* model_vertices = (float*)malloc(max_vertices * CHUNK_VERT_FLOATS * sizeof(float));
    int* model_indices = (int*)malloc(max_indices * sizeof(int));
    float* water_vertices = (float*)malloc(max_vertices * CHUNK_VERT_FLOATS * sizeof(float));
    int* water_indices = (int*)malloc(max_indices * sizeof(int));

    int v_cursor = 0, i_cursor = 0;
    int w_v_cursor = 0, w_i_cursor = 0;

    int wx_offset = cx * CHUNK_WIDTH;
    int wz_offset = cz * CHUNK_DEPTH;

    for (int x = 0; x < CHUNK_WIDTH; x++) {
        for (int y = 0; y < CHUNK_HEIGHT; y++) {
            for (int z = 0; z < CHUNK_DEPTH; z++) {
                int index = x + CHUNK_WIDTH * (y + CHUNK_HEIGHT * z);
                uint16_t tile_id = data[index];

                if (tile_id == AIR)
                    continue;

                int wx = wx_offset + x;
                int wy = y;
                int wz = wz_offset + z;

                float pos[3] = {(float)x, (float)y, (float)z};

                if (is_cross_block(tile_id)) {
                    float light = chunk_light_vertex(sky_light, block_light, x, y, z);
                    uint16_t front = chunk_neighbors_get(n, wx, wy, wz + 1);
                    uint16_t back  = chunk_neighbors_get(n, wx, wy, wz - 1);
                    uint16_t left  = chunk_neighbors_get(n, wx - 1, wy, wz);
                    uint16_t right = chunk_neighbors_get(n, wx + 1, wy, wz);
                    
                    if (lookup_transparent[front] || lookup_transparent[back] || 
                        lookup_transparent[left] || lookup_transparent[right]) {
                        
                        float uv[8];
                        
                        int atlas_tex1 = atlas_lookup(tile_id, FRONT);
                        tile_atlas_getuv(atlas_tex1, uv);
                        
                        float verts1[4][3] = {
                            {0.0f, 0.0f, 0.0f},
                            {1.0f, 0.0f, 1.0f},
                            {1.0f, 1.0f, 1.0f},
                            {0.0f, 1.0f, 0.0f}
                        };
                        
int v_start = v_cursor / CHUNK_VERT_FLOATS;
                        for (int i2 = 0; i2 < 4; i2++) {
                            int base = v_cursor;
                            model_vertices[base + 0] = verts1[i2][0] + pos[0];
                            model_vertices[base + 1] = verts1[i2][1] + pos[1];
                            model_vertices[base + 2] = verts1[i2][2] + pos[2];
                            model_vertices[base + 3] = uv[i2 * 2 + 0];
                            model_vertices[base + 4] = uv[i2 * 2 + 1];
                            model_vertices[base + 5] = 0.707f;
                            model_vertices[base + 6] = 0.0f;
                            model_vertices[base + 7] = 0.707f;
                            model_vertices[base + 8] = light;
                            model_vertices[base + 9] = 1.0f;
                            model_vertices[base + 10] = 0.0f;
                            model_vertices[base + 11] = 0.0f;
                            model_vertices[base + 12] = 0.0f;
                            model_vertices[base + 13] = 1.0f;
                            model_vertices[base + 14] = 0.0f;
                            v_cursor += CHUNK_VERT_FLOATS;
                        }

                        model_indices[i_cursor + 0] = v_start + 0;
                        model_indices[i_cursor + 1] = v_start + 1;
                        model_indices[i_cursor + 2] = v_start + 2;
                        model_indices[i_cursor + 3] = v_start + 2;
                        model_indices[i_cursor + 4] = v_start + 3;
                        model_indices[i_cursor + 5] = v_start + 0;
                        i_cursor += 6;

                        v_start = v_cursor / CHUNK_VERT_FLOATS;
                        for (int i2 = 0; i2 < 4; i2++) {
                            int base = v_cursor;
                            model_vertices[base + 0] = verts1[i2][0] + pos[0];
                            model_vertices[base + 1] = verts1[i2][1] + pos[1];
                            model_vertices[base + 2] = verts1[i2][2] + pos[2];
                            model_vertices[base + 3] = uv[i2 * 2 + 0];
                            model_vertices[base + 4] = uv[i2 * 2 + 1];
                            model_vertices[base + 5] = -0.707f;
                            model_vertices[base + 6] = 0.0f;
                            model_vertices[base + 7] = -0.707f;
                            model_vertices[base + 8] = light;
                            model_vertices[base + 9] = 1.0f;
                            model_vertices[base + 10] = 0.0f;
                            model_vertices[base + 11] = 0.0f;
                            model_vertices[base + 12] = 0.0f;
                            model_vertices[base + 13] = 1.0f;
                            model_vertices[base + 14] = 0.0f;
                            v_cursor += CHUNK_VERT_FLOATS;
                        }

                        model_indices[i_cursor + 0] = v_start + 0;
                        model_indices[i_cursor + 1] = v_start + 2;
                        model_indices[i_cursor + 2] = v_start + 1;
                        model_indices[i_cursor + 3] = v_start + 2;
                        model_indices[i_cursor + 4] = v_start + 0;
                        model_indices[i_cursor + 5] = v_start + 3;
                        i_cursor += 6;

                        int atlas_tex2 = atlas_lookup(tile_id, BACK);
                        tile_atlas_getuv(atlas_tex2, uv);

                        float verts2[4][3] = {
                            {1.0f, 0.0f, 0.0f},
                            {0.0f, 0.0f, 1.0f},
                            {0.0f, 1.0f, 1.0f},
                            {1.0f, 1.0f, 0.0f}
                        };

                        v_start = v_cursor / CHUNK_VERT_FLOATS;
                        for (int i2 = 0; i2 < 4; i2++) {
                            int base = v_cursor;
                            model_vertices[base + 0] = verts2[i2][0] + pos[0];
                            model_vertices[base + 1] = verts2[i2][1] + pos[1];
                            model_vertices[base + 2] = verts2[i2][2] + pos[2];
                            model_vertices[base + 3] = uv[i2 * 2 + 0];
                            model_vertices[base + 4] = uv[i2 * 2 + 1];
                            model_vertices[base + 5] = -0.707f;
                            model_vertices[base + 6] = 0.0f;
                            model_vertices[base + 7] = 0.707f;
                            model_vertices[base + 8] = light;
                            model_vertices[base + 9] = 1.0f;
                            model_vertices[base + 10] = 0.0f;
                            model_vertices[base + 11] = 0.0f;
                            model_vertices[base + 12] = 0.0f;
                            model_vertices[base + 13] = 1.0f;
                            model_vertices[base + 14] = 0.0f;
                            v_cursor += CHUNK_VERT_FLOATS;
                        }

                        model_indices[i_cursor + 0] = v_start + 0;
                        model_indices[i_cursor + 1] = v_start + 1;
                        model_indices[i_cursor + 2] = v_start + 2;
                        model_indices[i_cursor + 3] = v_start + 2;
                        model_indices[i_cursor + 4] = v_start + 3;
                        model_indices[i_cursor + 5] = v_start + 0;
                        i_cursor += 6;

                        v_start = v_cursor / CHUNK_VERT_FLOATS;
                        for (int i2 = 0; i2 < 4; i2++) {
                            int base = v_cursor;
                            model_vertices[base + 0] = verts2[i2][0] + pos[0];
                            model_vertices[base + 1] = verts2[i2][1] + pos[1];
                            model_vertices[base + 2] = verts2[i2][2] + pos[2];
                            model_vertices[base + 3] = uv[i2 * 2 + 0];
                            model_vertices[base + 4] = uv[i2 * 2 + 1];
                            model_vertices[base + 5] = 0.707f;
                            model_vertices[base + 6] = 0.0f;
                            model_vertices[base + 7] = -0.707f;
                            model_vertices[base + 8] = light;
                            model_vertices[base + 9] = 1.0f;
                            model_vertices[base + 10] = 0.0f;
                            model_vertices[base + 11] = 0.0f;
                            model_vertices[base + 12] = 0.0f;
                            model_vertices[base + 13] = 1.0f;
                            model_vertices[base + 14] = 0.0f;
                            v_cursor += CHUNK_VERT_FLOATS;
                        }

                        model_indices[i_cursor + 0] = v_start + 0;
                        model_indices[i_cursor + 1] = v_start + 2;
                        model_indices[i_cursor + 2] = v_start + 1;
                        model_indices[i_cursor + 3] = v_start + 2;
                        model_indices[i_cursor + 4] = v_start + 0;
                        model_indices[i_cursor + 5] = v_start + 3;
                        i_cursor += 6;

                    }
                } else {
                    push_face_to_buffer(tile_id, pos, sky_light, block_light, x, y, z, n, wx, wy, wz,
                                      model_vertices, model_indices, &v_cursor, &i_cursor,
                                      water_vertices, water_indices, &w_v_cursor, &w_i_cursor);
                }
            }
        }
    }

    out->model_verts = model_vertices;
    out->model_inds = model_indices;
    out->model_v = v_cursor;
    out->model_i = i_cursor;
    out->water_verts = water_vertices;
    out->water_inds = water_indices;
    out->water_v = w_v_cursor;
    out->water_i = w_i_cursor;

    shadow_dirty = 1;
}