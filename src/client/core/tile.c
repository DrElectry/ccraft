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

static uint32_t quant_light6(float l) {
    if (l < 0.0f) l = 0.0f;
    if (l > 1.0f) l = 1.0f;
    return (uint32_t)(l * 63.0f + 0.5f);
}

void tile_pack_face(ChunkFace* out, int x, int y, int z, int face, int atlas,
                    const float light[4], int flip, uint8_t special)
{
    uint32_t a = 0;
    a |= (uint32_t)x & 15u;
    a |= ((uint32_t)z & 15u) << 4;
    a |= ((uint32_t)y & 255u) << 8;
    a |= ((uint32_t)face & 15u) << 16;
    a |= ((uint32_t)atlas & 255u) << 20;
    if (flip)
        a |= 1u << 28;

    uint32_t b = 0;
    b |= quant_light6(light[0]);
    b |= quant_light6(light[1]) << 6;
    b |= quant_light6(light[2]) << 12;
    b |= quant_light6(light[3]) << 18;
    b |= (uint32_t)special << 24;

    out->a = a;
    out->b = b;
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
    float lights[4] = {light, light, light, light};
    tile_push_face_smooth(vertices, indices, pos, v_cursor, i_cursor,
                          face, atlas_id, lights, 0);
}

void tile_push_face_smooth(float* vertices,
                           unsigned int* indices,
                           float* pos,
                           int* v_cursor,
                           int* i_cursor,
                           int face,
                           int atlas_id,
                           const float light[4],
                           int flip)
{
    (void)indices;
    ChunkFace* faces = (ChunkFace*)vertices;
    tile_pack_face(&faces[*v_cursor],
                   (int)pos[0], (int)pos[1], (int)pos[2],
                   face, atlas_id, light, flip, 0);
    (*v_cursor)++;
    if (i_cursor)
        *i_cursor = *v_cursor;
}

void tile_push_cross(float* vertices,
                     unsigned int* indices,
                     float* pos,
                     int* v_cursor,
                     int* i_cursor,
                     int atlas_id,
                     float light)
{
    float lights[4] = {light, light, light, light};
    tile_push_face_smooth(vertices, indices, pos, v_cursor, i_cursor,
                          CROSS_A, atlas_id, lights, 0);
    tile_push_face_smooth(vertices, indices, pos, v_cursor, i_cursor,
                          CROSS_B, atlas_id, lights, 0);
}

void tile_push_cube(float* vertices, unsigned int* indices, float* pos, int* v_cursor, int* i_cursor) {
    tile_push_face(vertices, indices, pos, v_cursor, i_cursor, FRONT, 0, 1.0f);
    tile_push_face(vertices, indices, pos, v_cursor, i_cursor, BACK, 0, 1.0f);
    tile_push_face(vertices, indices, pos, v_cursor, i_cursor, RIGHT, 0, 1.0f);
    tile_push_face(vertices, indices, pos, v_cursor, i_cursor, LEFT, 0, 1.0f);
    tile_push_face(vertices, indices, pos, v_cursor, i_cursor, UP, 0, 1.0f);
    tile_push_face(vertices, indices, pos, v_cursor, i_cursor, DOWN, 0, 1.0f);
}

Render_request tile_render_cache[19];

void tile_create_cube_from_tile(uint16_t block_id, Render_request* out)
{
    int is_cross = 0;
    if (lookup_cross[block_id] == 1) {
        is_cross = 1;
    }
    
    int max_faces = is_cross ? 2 : 6;

    ChunkFace* verts = (ChunkFace*)malloc((size_t)max_faces * sizeof(ChunkFace));
    int v_cursor = 0;
    int i_cursor = 0;

    float pos[3] = {0.0f, 0.0f, 0.0f};

    if (is_cross) {
        int atlas_id = lookup_atlas[block_id * 6 + FRONT];
        tile_push_cross((float*)verts, NULL, pos, &v_cursor, &i_cursor, atlas_id, 1.0f);
    } else {
        int atlas_id;

        atlas_id = lookup_atlas[block_id * 6 + FRONT];
        tile_push_face((float*)verts, NULL, pos, &v_cursor, &i_cursor, FRONT, atlas_id, 1.0f);

        atlas_id = lookup_atlas[block_id * 6 + BACK];
        tile_push_face((float*)verts, NULL, pos, &v_cursor, &i_cursor, BACK, atlas_id, 1.0f);

        atlas_id = lookup_atlas[block_id * 6 + RIGHT];
        tile_push_face((float*)verts, NULL, pos, &v_cursor, &i_cursor, RIGHT, atlas_id, 1.0f);

        atlas_id = lookup_atlas[block_id * 6 + LEFT];
        tile_push_face((float*)verts, NULL, pos, &v_cursor, &i_cursor, LEFT, atlas_id, 1.0f);

        atlas_id = lookup_atlas[block_id * 6 + UP];
        tile_push_face((float*)verts, NULL, pos, &v_cursor, &i_cursor, UP, atlas_id, 1.0f);

        atlas_id = lookup_atlas[block_id * 6 + DOWN];
        tile_push_face((float*)verts, NULL, pos, &v_cursor, &i_cursor, DOWN, atlas_id, 1.0f);
    }

    out->data = (float*)verts;
    out->triangles = NULL;
    out->data_size = v_cursor * (int)sizeof(ChunkFace);
    out->tri_count = v_cursor;
    out->packed = 1;

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
        glDrawArraysInstanced(GL_TRIANGLES, 0, 6, tile_render_cache[id].tri_count);

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