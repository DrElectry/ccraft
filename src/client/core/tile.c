#include "core/tile.h"
#include "core/chunk.h"
#include "core/gfx.h"
#include "gl/fbo.h"
#include "core/game.h"
#include "core/main.h"
#include <stdlib.h>
#include <cglm/cglm.h>

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

const float cross_vertices[] = {
    -0.5f, 0.0f, -0.5f,  0.5f, 0.0f, 0.5f,  0.5f, 1.0f, 0.5f,  -0.5f, 1.0f, -0.5f,
     0.5f, 0.0f, -0.5f, -0.5f, 0.0f, 0.5f, -0.5f, 1.0f, 0.5f,   0.5f, 1.0f, -0.5f
};

const unsigned int cross_indices[] = {
    0,1,2, 2,3,0,
    4,5,6, 6,7,4
};

const float cross_normals[] = {
    0.707f, 0.0f, 0.707f,
   -0.707f, 0.0f, 0.707f
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
                    int atlas_id,
                    float light)

{
    float uv[8];
    tile_atlas_getuv(atlas_id, uv);

    float nx = face_normals[face * 3 + 0];
    float ny = face_normals[face * 3 + 1];
    float nz = face_normals[face * 3 + 2];

    float tx = 0.0f, ty = 0.0f, tz = 0.0f;
    float bx = 0.0f, by = 0.0f, bz = 0.0f;

    if (face == FRONT) {
        tx = 1.0f; ty = 0.0f; tz = 0.0f;
        bx = 0.0f; by = 1.0f; bz = 0.0f;
    } else if (face == BACK) {
        tx = -1.0f; ty = 0.0f; tz = 0.0f;
        bx = 0.0f; by = 1.0f; bz = 0.0f;
    } else if (face == RIGHT) {
        tx = 0.0f; ty = 0.0f; tz = -1.0f;
        bx = 0.0f; by = 1.0f; bz = 0.0f;
    } else if (face == LEFT) {
        tx = 0.0f; ty = 0.0f; tz = 1.0f;
        bx = 0.0f; by = 1.0f; bz = 0.0f;
    } else if (face == UP) {
        tx = 1.0f; ty = 0.0f; tz = 0.0f;
        bx = 0.0f; by = 0.0f; bz = -1.0f;
    } else {
        tx = 1.0f; ty = 0.0f; tz = 0.0f;
        bx = 0.0f; by = 0.0f; bz = 1.0f;
    }

    int v_start = *v_cursor / CHUNK_VERT_FLOATS;
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
        vertices[base + 8] = light;

        vertices[base + 9] = tx;
        vertices[base + 10] = ty;
        vertices[base + 11] = tz;

        vertices[base + 12] = bx;
        vertices[base + 13] = by;
        vertices[base + 14] = bz;

        *v_cursor += CHUNK_VERT_FLOATS;
    }

    indices[*i_cursor + 0] = v_start + 0;
    indices[*i_cursor + 1] = v_start + 1;
    indices[*i_cursor + 2] = v_start + 2;
    indices[*i_cursor + 3] = v_start + 2;
    indices[*i_cursor + 4] = v_start + 3;
    indices[*i_cursor + 5] = v_start + 0;

    *i_cursor += 6;
}

void tile_push_cross_face(float* vertices,
                          unsigned int* indices,
                          float* pos,
                          int* v_cursor,
                          int* i_cursor,
                          int cross_face,
                          int atlas_id,
                          float light)
{
    float uv[8];
    tile_atlas_getuv(atlas_id, uv);

    float nx = cross_normals[cross_face * 3 + 0];
    float ny = cross_normals[cross_face * 3 + 1];
    float nz = cross_normals[cross_face * 3 + 2];

    float tx, ty, tz, bx, by, bz;
    if (cross_face == 0) {
        tx = 1.0f; ty = 0.0f; tz = 0.0f;
        bx = 0.0f; by = 1.0f; bz = 0.0f;
    } else {
        tx = 0.0f; ty = 0.0f; tz = 1.0f;
        bx = 0.0f; by = 1.0f; bz = 0.0f;
    }

    int v_start = *v_cursor / CHUNK_VERT_FLOATS;
    int offset = cross_face * 12;

    for (int i = 0; i < 4; i++)
    {
        int base = *v_cursor;

        vertices[base + 0] = cross_vertices[offset + i * 3 + 0] + pos[0];
        vertices[base + 1] = cross_vertices[offset + i * 3 + 1] + pos[1];
        vertices[base + 2] = cross_vertices[offset + i * 3 + 2] + pos[2];

        vertices[base + 3] = uv[i * 2 + 0];
        vertices[base + 4] = uv[i * 2 + 1];

        vertices[base + 5] = nx;
        vertices[base + 6] = ny;
        vertices[base + 7] = nz;
        vertices[base + 8] = light;

        vertices[base + 9] = tx;
        vertices[base + 10] = ty;
        vertices[base + 11] = tz;

        vertices[base + 12] = bx;
        vertices[base + 13] = by;
        vertices[base + 14] = bz;

        *v_cursor += CHUNK_VERT_FLOATS;
    }

    indices[*i_cursor + 0] = v_start + 0;
    indices[*i_cursor + 1] = v_start + 1;
    indices[*i_cursor + 2] = v_start + 2;
    indices[*i_cursor + 3] = v_start + 2;
    indices[*i_cursor + 4] = v_start + 3;
    indices[*i_cursor + 5] = v_start + 0;

    *i_cursor += 6;
}

void tile_push_cross(float* vertices,
                     unsigned int* indices,
                     float* pos,
                     int* v_cursor,
                     int* i_cursor,
                     int atlas_id,
                     float light)
{
    tile_push_cross_face(vertices, indices, pos, v_cursor, i_cursor, 0, atlas_id, light);
    tile_push_cross_face(vertices, indices, pos, v_cursor, i_cursor, 1, atlas_id, light);
}

void tile_push_cube(float* vertices, unsigned int* indices, float* pos, int* v_cursor, int* i_cursor) {
    tile_push_face(vertices, indices, pos, v_cursor, i_cursor, FRONT, 0, 15);
    tile_push_face(vertices, indices, pos, v_cursor, i_cursor, BACK, 0, 15);
    tile_push_face(vertices, indices, pos, v_cursor, i_cursor, RIGHT, 0, 15);
    tile_push_face(vertices, indices, pos, v_cursor, i_cursor, LEFT, 0, 15);
    tile_push_face(vertices, indices, pos, v_cursor, i_cursor, UP, 0, 15);
    tile_push_face(vertices, indices, pos, v_cursor, i_cursor, DOWN, 0, 15);
}

Render_request tile_render_cache[19];

void tile_create_cube_from_tile(uint16_t block_id, Render_request* out)
{
    int is_cross = 0;
    if (lookup_cross[block_id] == 1) {
        is_cross = 1;
    }
    
    int max_faces = is_cross ? 2 : 6;
    const int max_vertices = max_faces * 4;
    const int max_indices = max_faces * 6;

    float* verts = (float*)malloc(max_vertices * CHUNK_VERT_FLOATS * sizeof(float));
    int* inds = (int*)malloc(max_indices * sizeof(int));

    int v_cursor = 0;
    int i_cursor = 0;

    float pos[3] = {0.0f, 0.0f, 0.0f};

    if (is_cross) {
        int atlas_id = lookup_atlas[block_id * 6 + FRONT];
        tile_push_cross(verts, (unsigned int*)inds, pos, &v_cursor, &i_cursor, atlas_id, 15.0f);
    } else {
        int atlas_id;

        atlas_id = lookup_atlas[block_id * 6 + FRONT];
        tile_push_face(verts, (unsigned int*)inds, pos, &v_cursor, &i_cursor, FRONT, atlas_id, 15.0f);

        atlas_id = lookup_atlas[block_id * 6 + BACK];
        tile_push_face(verts, (unsigned int*)inds, pos, &v_cursor, &i_cursor, BACK, atlas_id, 15.0f);

        atlas_id = lookup_atlas[block_id * 6 + RIGHT];
        tile_push_face(verts, (unsigned int*)inds, pos, &v_cursor, &i_cursor, RIGHT, atlas_id, 15.0f);

        atlas_id = lookup_atlas[block_id * 6 + LEFT];
        tile_push_face(verts, (unsigned int*)inds, pos, &v_cursor, &i_cursor, LEFT, atlas_id, 15.0f);

        atlas_id = lookup_atlas[block_id * 6 + UP];
        tile_push_face(verts, (unsigned int*)inds, pos, &v_cursor, &i_cursor, UP, atlas_id, 15.0f);

        atlas_id = lookup_atlas[block_id * 6 + DOWN];
        tile_push_face(verts, (unsigned int*)inds, pos, &v_cursor, &i_cursor, DOWN, atlas_id, 15.0f);
    }

    out->data = verts;
    out->triangles = inds;
    out->data_size = v_cursor * sizeof(float);
    out->tri_count = i_cursor / 3;

    glm_vec3_copy((vec3){0.0f, 0.0f, 0.0f}, out->pos);
    glm_vec3_copy((vec3){0.0f, 0.0f, 0.0f}, out->rot);
    glm_vec3_copy((vec3){1.0f, 1.0f, 1.0f}, out->scale);
}

FBO tile_fbos[19] = {0};
Canvas_Render_Request tile_icons[19] = {0};

void tile_pre_render_all(Program* prog, Texture* atlas, Texture* roug_tex, Texture* norm_tex)
{
    for (uint16_t id = FIRST_TILE; id <= LAST_TILE; id++) {
        tile_create_cube_from_tile(id, &tile_render_cache[id]);
        gfx_chunk_packet_static_request(&tile_render_cache[id]);

        fbo_free(&tile_fbos[id]);

        FBO* fbo = &tile_fbos[id];
        fbo->color_formats[0] = FBO_COLOR_RGBA16F;
        fbo->color_formats[1] = FBO_COLOR_RGB16F;
        fbo->color_formats[2] = FBO_COLOR_RGBA16F;
        fbo->color_formats[3] = FBO_COLOR_RG16F;
        fbo_create(fbo, TILE_ICON_SIZE, TILE_ICON_SIZE, 4);
        fbo_bind(fbo);

        glViewport(0, 0, TILE_ICON_SIZE, TILE_ICON_SIZE);
        glClearColor(1.0f, 0.0f, 1.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        mat4 proj, view, model;
        glm_mat4_identity(proj);
        glm_ortho(-1.2f, 1.2f, -1.2f, 1.2f, -5.0f, 5.0f, proj);

        glm_mat4_identity(view);
        vec3 eye = {1.0f, 1.2f, 1.0f};
        vec3 center = {0.0f, 0.0f, 0.0f};
        vec3 up = {0.0f, 1.0f, 0.0f};
        glm_lookat(eye, center, up, view);

        glm_mat4_identity(model);

        program_use(prog);
        texture_bind(atlas, 0);
        texture_bind(roug_tex, 1);
        texture_bind(norm_tex, 2);
        program_set_int(prog, "tex", 0);
        program_set_int(prog, "roug", 1);
        program_set_int(prog, "normal", 2);
        program_set_mat4(prog, "proj", (float*)proj);
        program_set_mat4(prog, "view", (float*)view);
        program_set_mat4(prog, "model", (float*)model);

        glEnable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);

        vao_bind(&tile_render_cache[id].cache.vao);
        glDrawElements(GL_TRIANGLES, tile_render_cache[id].tri_count * 3, GL_UNSIGNED_INT, NULL);

        glEnable(GL_CULL_FACE);

        fbo_unbind();
        glViewport(0, 0, WIDTH, HEIGHT);

        glm_vec2_copy((vec2){0.0f, 0.0f}, tile_icons[id].pos);
        glm_vec2_copy((vec2){TILE_ICON_SIZE, TILE_ICON_SIZE}, tile_icons[id].scale);
        tile_icons[id].rotation = 0.0f;
        tile_icons[id].alpha = 1.0f;
        gfx_canvas_packet_static_request(&tile_icons[id]);

        glClearColor(0.6f, 0.7f, 0.8f, 1.0f);
    }
}