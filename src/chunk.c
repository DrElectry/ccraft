#include "chunk.h"
#include <cglm/cglm.h>
#include "tile.h"
#include "gfx.h"
#include "world.h"

#include "rand.h"

#include <stdint.h>
#include <stdlib.h>

int lookup_atlas[] = { // strict 6 ints per block FRONT BACK LEFT RIGHT UP DOWN
    0,0,0,0,0,0, // air has zero standards
    1,1,1,1,0,2, // grass
    2,2,2,2,2,2, // dirt
};

inline int atlas_lookup(uint16_t tile_id, enum Tile_face face)
{
    return lookup_atlas[tile_id * 6 + face];
}

void chunk_generate(Chunk* chunk) {
    chunk->data = (uint16_t*)malloc(
        CHUNK_WIDTH * CHUNK_HEIGHT * CHUNK_DEPTH * sizeof(uint16_t)
    );

    for (int x = 0; x < CHUNK_WIDTH; x++) {
        for (int y = 0; y < CHUNK_HEIGHT; y++) {
            for (int z = 0; z < CHUNK_DEPTH; z++) {
                int index = x + CHUNK_WIDTH * (y + CHUNK_HEIGHT * z);
                if (y == 8) {
                    chunk->data[index] = GRASS;
                } else if (y < 8) {
                    chunk->data[index] = DIRT;
                } else {
                    chunk->data[index] = AIR;
                }
            }
        }
    }

    for (int x = 0; x < CHUNK_WIDTH; x++) {
        for (int z = 0; z < CHUNK_DEPTH; z++) {
            int index = x + CHUNK_WIDTH * (9 + CHUNK_HEIGHT * z);
            if (RAND(0, 16) == 0) {
                chunk->data[index] = DIRT;
            }
        }
    }

    chunk->model = (Render_request){0};
}

void chunk_rebuild(Chunk* chunk, struct World* world, int cx, int cz) {
    if (chunk->model.data) { // freeing garbage cpu buffers
        free(chunk->model.data);
        chunk->model.data = NULL;
    }
    if (chunk->model.triangles) {
        free(chunk->model.triangles);
        chunk->model.triangles = NULL;
    }

    // and previous gpu buffers too
    vbo_free(&chunk->model.cache.vbo);
    vao_free(&chunk->model.cache.vao);
    ebo_free(&chunk->model.cache.ebo);

    int max_blocks = CHUNK_WIDTH * CHUNK_HEIGHT * CHUNK_DEPTH;
    int max_faces  = max_blocks * 6;
    int max_vertices = max_faces * 4;
    int max_indices  = max_faces * 6;

    float* vertices = (float*)malloc(max_vertices * 5 * sizeof(float));
    int*   indices  = (int*)malloc(max_indices * sizeof(int));

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

                // +z (FRONT)
                if (world_get_block(world, wx, wy, wz + 1) == AIR)
                    tile_push_face(vertices, (unsigned int*)indices, (float[]){(float)x, (float)y, (float)z}, &v_cursor, &i_cursor, FRONT, atlas_lookup(tile_id, FRONT));

                // -z (BACK)
                if (world_get_block(world, wx, wy, wz - 1) == AIR)
                    tile_push_face(vertices, (unsigned int*)indices, (float[]){(float)x, (float)y, (float)z}, &v_cursor, &i_cursor, BACK, atlas_lookup(tile_id, BACK));

                // -x (LEFT)
                if (world_get_block(world, wx - 1, wy, wz) == AIR)
                    tile_push_face(vertices, (unsigned int*)indices, (float[]){(float)x, (float)y, (float)z}, &v_cursor, &i_cursor, LEFT, atlas_lookup(tile_id, LEFT));

                // +x (RIGHT)
                if (world_get_block(world, wx + 1, wy, wz) == AIR)
                    tile_push_face(vertices, (unsigned int*)indices, (float[]){(float)x, (float)y, (float)z}, &v_cursor, &i_cursor, RIGHT, atlas_lookup(tile_id, RIGHT));

                // +y (UP)
                if (world_get_block(world, wx, wy + 1, wz) == AIR)
                    tile_push_face(vertices, (unsigned int*)indices, (float[]){(float)x, (float)y, (float)z}, &v_cursor, &i_cursor, UP, atlas_lookup(tile_id, UP));

                // -y (DOWN)
                if (world_get_block(world, wx, wy - 1, wz) == AIR)
                    tile_push_face(vertices, (unsigned int*)indices, (float[]){(float)x, (float)y, (float)z}, &v_cursor, &i_cursor, DOWN, atlas_lookup(tile_id, DOWN));
            }
        }
    }

    if (v_cursor > 0) {
        vertices = (float*)realloc(vertices, v_cursor * sizeof(float));
        indices  = (int*)realloc(indices, i_cursor * sizeof(int));

        chunk->model.data = vertices;
        chunk->model.triangles = indices;
        chunk->model.data_size = v_cursor * sizeof(float);
        chunk->model.tri_count = i_cursor / 3;

        chunk->model.rot = 0.0f;
        glm_vec3_copy((vec3){1.0f, 1.0f, 1.0f}, chunk->model.scale);

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

