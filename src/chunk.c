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

#define SEA_LEVEL 8
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
};

int lookup_transparent[] = {
    1, // AIR
    0, // GRASS
    0, // DIRT
    1, // LEAVES
    0, // STONE
    0, // IRON_BLOCK
    1, // WATER
    0, // LOG
};

inline int atlas_lookup(uint16_t tile_id, enum Tile_face face)
{
    return lookup_atlas[tile_id * 6 + face];
}

static int get_terrain_height(int wx, int wz) {
    float h = fbm2d((float)wx * 0.015f, (float)wz * 0.015f, 4, 0.5f, 2.0f);
    float detail = fbm2d((float)wx * 0.08f, (float)wz * 0.08f, 2, 0.5f, 2.0f) * 0.25f;
    int height = SEA_LEVEL + (int)((h + detail) * 1.0f);
    return height;
}

static uint16_t get_block_at_height(int wy, int terrain_height, int water_level) {
    if (wy > terrain_height) {
        return AIR;
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
    chunk->data = (uint16_t*)malloc(
        CHUNK_WIDTH * CHUNK_HEIGHT * CHUNK_DEPTH * sizeof(uint16_t)
    );

    for (int x = 0; x < CHUNK_WIDTH; x++) {
        for (int z = 0; z < CHUNK_DEPTH; z++) {
            int wx = gen_chunk_x * CHUNK_WIDTH + x;
            int wz = gen_chunk_z * CHUNK_DEPTH + z;

            int terrain_height = get_terrain_height(wx, wz);

            for (int y = 0; y < CHUNK_HEIGHT; y++) {
                int index = x + CHUNK_WIDTH * (y + CHUNK_HEIGHT * z);
                uint16_t block = get_block_at_height(y, terrain_height, SEA_LEVEL);
                chunk->data[index] = block;
            }
        }
    }

chunk->model = (Render_request){0};
    chunk->water_model = (Render_request){0};
}

// Helper to check if a block type is in a category
static inline int is_opaque_block(uint16_t tile_id) {
    return tile_id != AIR && tile_id != LEAVES && tile_id != WATER;
}

static inline int is_water_block(uint16_t tile_id) {
    return tile_id == WATER;
}



// Helper to free render request data
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

// Helper to finalize a render request
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

    // Check if front face should be rendered
    int render_front = lookup_transparent[front] || (is_water_block(tile_id) && !is_opaque_block(front));
    if (render_front) {
        if (is_water_block(tile_id)) {
            tile_push_face(water_verts, (unsigned int*)water_inds, pos, w_v_cur, w_i_cur, FRONT, atlas_lookup(tile_id, FRONT));
        } else {
            tile_push_face(model_verts, (unsigned int*)model_inds, pos, v_cur, i_cur, FRONT, atlas_lookup(tile_id, FRONT));
        }
    }

    int render_back = lookup_transparent[back] || (is_water_block(tile_id) && !is_opaque_block(back));
    if (render_back) {
        if (is_water_block(tile_id)) {
            tile_push_face(water_verts, (unsigned int*)water_inds, pos, w_v_cur, w_i_cur, BACK, atlas_lookup(tile_id, BACK));
        } else {
            tile_push_face(model_verts, (unsigned int*)model_inds, pos, v_cur, i_cur, BACK, atlas_lookup(tile_id, BACK));
        }
    }

    int render_left = lookup_transparent[left] || (is_water_block(tile_id) && !is_opaque_block(left));
    if (render_left) {
        if (is_water_block(tile_id)) {
            tile_push_face(water_verts, (unsigned int*)water_inds, pos, w_v_cur, w_i_cur, LEFT, atlas_lookup(tile_id, LEFT));
        } else {
            tile_push_face(model_verts, (unsigned int*)model_inds, pos, v_cur, i_cur, LEFT, atlas_lookup(tile_id, LEFT));
        }
    }

    int render_right = lookup_transparent[right] || (is_water_block(tile_id) && !is_opaque_block(right));
    if (render_right) {
        if (is_water_block(tile_id)) {
            tile_push_face(water_verts, (unsigned int*)water_inds, pos, w_v_cur, w_i_cur, RIGHT, atlas_lookup(tile_id, RIGHT));
        } else {
            tile_push_face(model_verts, (unsigned int*)model_inds, pos, v_cur, i_cur, RIGHT, atlas_lookup(tile_id, RIGHT));
        }
    }

    int render_up = lookup_transparent[up] || (is_water_block(tile_id) && !is_opaque_block(up));
    if (render_up) {
        if (is_water_block(tile_id)) {
            tile_push_face(water_verts, (unsigned int*)water_inds, pos, w_v_cur, w_i_cur, UP, atlas_lookup(tile_id, UP));
        } else {
            tile_push_face(model_verts, (unsigned int*)model_inds, pos, v_cur, i_cur, UP, atlas_lookup(tile_id, UP));
        }
    }

    int render_down = lookup_transparent[down] || (is_water_block(tile_id) && !is_opaque_block(down));
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
