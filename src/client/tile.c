#include "tile.h"

const float face_vertices[] = {
    0,0,1, 1,0,1, 1,1,1, 0,1,1,
    1,0,0, 0,0,0, 0,1,0, 1,1,0,
    0,0,0, 0,0,1, 0,1,1, 0,1,0,
    1,0,1, 1,0,0, 1,1,0, 1,1,1,
    0,1,1, 1,1,1, 1,1,0, 0,1,0,
    0,0,0, 1,0,0, 1,0,1, 0,0,1
};

const unsigned int face_indices[] = {
    0,1,2, 2,3,0,
    4,5,6, 6,7,4,
    8,9,10, 10,11,8,
    12,13,14, 14,15,12,
    16,17,18, 18,19,16,
    20,21,22, 22,23,20
};

const float face_normals[] = {
    0,0,1,
    0,0,-1,
    -1,0,0,
    1,0,0,
    0,1,0,
    0,-1,0
};

void tile_atlas_getuv(int atlas_number, float* uv)
{
    const int tiles_per_row = 16;
    const float tile_size = 1.0f / tiles_per_row;

    int col = atlas_number % tiles_per_row;
    int row = atlas_number / tiles_per_row;

    float u0 = col * tile_size;
    float v0 = row * tile_size;
    float u1 = u0 + tile_size;
    float v1 = v0 + tile_size;

    uv[0] = u0; uv[1] = v1;
    uv[2] = u1; uv[3] = v1;
    uv[4] = u1; uv[5] = v0;
    uv[6] = u0; uv[7] = v0;
}

void tile_push_face(float* vertices,
                    unsigned int* indices,
                    float* pos,
                    int* v_cursor,
                    int* i_cursor,
                    int face,
                    int atlas_id)
{
    float uv[8];
    tile_atlas_getuv(atlas_id, uv);

    float nx = face_normals[face * 3 + 0];
    float ny = face_normals[face * 3 + 1];
    float nz = face_normals[face * 3 + 2];

    int v_start = *v_cursor / 8;
    int offset = face * 12;

    for (int i = 0; i < 4; i++)
    {
        int base = *v_cursor;

        vertices[base + 0] = face_vertices[offset + i * 3 + 0] + pos[0];
        vertices[base + 1] = face_vertices[offset + i * 3 + 1] + pos[1];
        vertices[base + 2] = face_vertices[offset + i * 3 + 2] + pos[2];

        vertices[base + 3] = uv[i * 2 + 0];
        vertices[base + 4] = uv[i * 2 + 1];

        vertices[base + 5] = nx;
        vertices[base + 6] = ny;
        vertices[base + 7] = nz;

        *v_cursor += 8;
    }

    indices[*i_cursor + 0] = v_start + 0;
    indices[*i_cursor + 1] = v_start + 1;
    indices[*i_cursor + 2] = v_start + 2;
    indices[*i_cursor + 3] = v_start + 2;
    indices[*i_cursor + 4] = v_start + 3;
    indices[*i_cursor + 5] = v_start + 0;

    *i_cursor += 6;
}