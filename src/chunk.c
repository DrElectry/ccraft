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
    0,0,0,0,0,0,
    1,1,1,1,0,2,
    2,2,2,2,2,2,
    3,3,3,3,3,3,
    4,4,4,4,4,4,
    5,5,5,5,5,5
};

int lookup_transparent[] = {
    1,
    0,
    0,
    1,
    0,
    0
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
}

void chunk_rebuild(Chunk* chunk, struct World* world, int cx, int cz) {
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

    int max_blocks = CHUNK_WIDTH * CHUNK_HEIGHT * CHUNK_DEPTH;
    int max_faces = max_blocks * 6;
    int max_vertices = max_faces * 4;
    int max_indices = max_faces * 6;

    float* vertices = (float*)malloc(max_vertices * 5 * sizeof(float));
    int* indices = (int*)malloc(max_indices * sizeof(int));

    int v_cursor = 0;
    int i_cursor = 0;

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

                uint16_t front = world_get_block(world, wx, wy, wz + 1);
                uint16_t back  = world_get_block(world, wx, wy, wz - 1);
                uint16_t left  = world_get_block(world, wx - 1, wy, wz);
                uint16_t right = world_get_block(world, wx + 1, wy, wz);
                uint16_t up    = world_get_block(world, wx, wy + 1, wz);
                uint16_t down  = world_get_block(world, wx, wy - 1, wz);

                if (lookup_transparent[front])
                    tile_push_face(vertices, (unsigned int*)indices,
                        (float[]){(float)x,(float)y,(float)z},
                        &v_cursor,&i_cursor,FRONT,
                        atlas_lookup(tile_id, FRONT));

                if (lookup_transparent[back])
                    tile_push_face(vertices, (unsigned int*)indices,
                        (float[]){(float)x,(float)y,(float)z},
                        &v_cursor,&i_cursor,BACK,
                        atlas_lookup(tile_id, BACK));

                if (lookup_transparent[left])
                    tile_push_face(vertices, (unsigned int*)indices,
                        (float[]){(float)x,(float)y,(float)z},
                        &v_cursor,&i_cursor,LEFT,
                        atlas_lookup(tile_id, LEFT));

                if (lookup_transparent[right])
                    tile_push_face(vertices, (unsigned int*)indices,
                        (float[]){(float)x,(float)y,(float)z},
                        &v_cursor,&i_cursor,RIGHT,
                        atlas_lookup(tile_id, RIGHT));

                if (lookup_transparent[up])
                    tile_push_face(vertices, (unsigned int*)indices,
                        (float[]){(float)x,(float)y,(float)z},
                        &v_cursor,&i_cursor,UP,
                        atlas_lookup(tile_id, UP));

                if (lookup_transparent[down])
                    tile_push_face(vertices, (unsigned int*)indices,
                        (float[]){(float)x,(float)y,(float)z},
                        &v_cursor,&i_cursor,DOWN,
                        atlas_lookup(tile_id, DOWN));
            }
        }
    }

    if (v_cursor > 0) {
        vertices = (float*)realloc(vertices, v_cursor * sizeof(float));
        indices = (int*)realloc(indices, i_cursor * sizeof(int));

        chunk->model.data = vertices;
        chunk->model.triangles = indices;
        chunk->model.data_size = v_cursor * sizeof(float);
        chunk->model.tri_count = i_cursor / 3;

        glm_vec3_copy((vec3){0.0f,0.0f,0.0f}, chunk->model.rot);
        glm_vec3_copy((vec3){1.0f,1.0f,1.0f}, chunk->model.scale);

        gfx_packet_static_request(&chunk->model);
    } else {
        free(vertices);
        free(indices);

        chunk->model.data = NULL;
        chunk->model.triangles = NULL;
        chunk->model.data_size = 0;
        chunk->model.tri_count = 0;
    }
}