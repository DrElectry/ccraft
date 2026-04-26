#ifndef GFX_H
#define GFX_H

#include "ebo.h"
#include "vao.h"
#include "vbo.h"
#include "shader.h"
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
    float rot;
    vec3 scale;

    GPUBuffer cache;
} Render_request;

void gfx_packet_static_request(Render_request* r); // call this whenever you want, it will just cache models ready to render
void gfx_render(Render_request *r, Program* active_program); // render pre cached model

#endif
