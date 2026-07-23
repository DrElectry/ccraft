#ifndef GFX_H
#define GFX_H

#include "gl/ebo.h"
#include "gl/vao.h"
#include "gl/vbo.h"
#include "gl/shader.h"

#include "core/skin.h"

#include <cglm/cglm.h>

typedef struct {
    VBO vbo;
    VAO vao;
    EBO ebo;
} GPUBuffer;

typedef struct {
    float* data;
    int* triangles;

    int data_size;
    int tri_count;

    vec3 pos;
    vec3 rot;
    vec3 scale;

    GPUBuffer cache;
} Render_request;

typedef struct {
    float* data;
    int* triangles;

    int data_size;
    int tri_count;

    Skinned* skinned;
    Skeleton* skeleton;
    AnimState* anim;

    SkinnedLook look;
} Skinned_render_request;

typedef struct {
    vec2 pos;
    vec2 scale;
    float rotation;
    
    GPUBuffer cache;
    int initialized;
} Canvas_Render_Request;

void gfx_packet_static_request(Render_request* r); // call this whenever you want, it will just cache models ready to render
void gfx_render(Render_request *r, Program* active_program); // render pre cached model
void gfx_skinned_render(Skinned_render_request* r, Program* active_program);

void gfx_program_create(Program* a, char* vsrc, char* fsc);
void gfx_chunk_packet_static_request(Render_request* r); // for chunks (they have lightmaps)

void gfx_draw_fullscreen_quad();

void gfx_canvas_packet_static_request(Canvas_Render_Request* r);
void gfx_canvas_render(Canvas_Render_Request* r, Program* active_program);
void gfx_canvas_render_batch(Canvas_Render_Request* requests, int count, Program* active_program);
void gfx_set_screen_projection(Program* program);

#endif