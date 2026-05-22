#include "text.h"
#include "gfx.h"
#include "vao.h"
#include "vbo.h"
#include "ebo.h"
#include "tex.h"
#include "fm.h"
#include "win.h"
#include "shader.h"
#include "log.h"

#include <cglm/cglm.h>
#include <string.h>
#include <stdlib.h>

#define CHAR_WIDTH 16
#define CHAR_HEIGHT 16
#define ATLAS_COLS 16
#define ATLAS_ROWS 16

static const char CHARSET[] =
"ABCDEFGHIJKLMNOPQRSTUVWXYZ!/<>:+1234567890&=().? ";

static int char_index(unsigned char c) {
    for (int i = 0; CHARSET[i] != '\0'; i++) {
        if (CHARSET[i] == c)
            return i;
    }
    return -1;
}

Texture text_atlas;
Program text_program;

void text_init(const char* vsrc, const char* fsrc, const char* asrc) {
    File v = file_open(vsrc);
    File f = file_open(fsrc);

    Shader a, b;

    a.type = GL_VERTEX_SHADER;
    b.type = GL_FRAGMENT_SHADER;

    shader_create(&a, v.data);
    shader_create(&b, f.data);

    text_atlas.mag_filter = GL_NEAREST;
    text_atlas.min_filter = GL_NEAREST;

    text_atlas.wrap_s = GL_REPEAT;
    text_atlas.wrap_t = GL_REPEAT;

    texture_create(&text_atlas, (char*)asrc);

    program_create(&text_program, &a, &b);
}

void text_create(HText* text, char* string, uint16_t color, int x, int y) {
    text->color = color;
    text->data = string;
    text->x = x;
    text->y = y;

    int len = (int)strlen(string);
    if (len == 0) {
        text->index_count = 0;
        return;
    }

    int vertex_count = len * 4;
    int index_count = len * 6;

    float* vertices = malloc(vertex_count * 4 * sizeof(float));
    int* indices = malloc(index_count * sizeof(int));

    for (int i = 0; i < len; i++) {
        unsigned char c = (unsigned char)string[i];
        int index = char_index(c);
        if (index == -1) index = 0;

        int grid_x = index % ATLAS_COLS;
        int grid_y = index / ATLAS_COLS;
        grid_y = ATLAS_ROWS - 1 - grid_y;

        float px = (float)(x + i * CHAR_WIDTH);
        float py = (float)y;

        float u1 = (float)grid_x / ATLAS_COLS;
        float v1 = 1.0f - (float)grid_y / ATLAS_ROWS;
        float u2 = (float)(grid_x + 1) / ATLAS_COLS;
        float v2 = 1.0f - (float)(grid_y + 1) / ATLAS_ROWS;

        int base = i * 16;

        vertices[base + 0] = px;
        vertices[base + 1] = py;
        vertices[base + 2] = u1;
        vertices[base + 3] = v2;

        vertices[base + 4] = px;
        vertices[base + 5] = py + CHAR_HEIGHT;
        vertices[base + 6] = u1;
        vertices[base + 7] = v1;

        vertices[base + 8] = px + CHAR_WIDTH;
        vertices[base + 9] = py + CHAR_HEIGHT;
        vertices[base + 10] = u2;
        vertices[base + 11] = v1;

        vertices[base + 12] = px + CHAR_WIDTH;
        vertices[base + 13] = py;
        vertices[base + 14] = u2;
        vertices[base + 15] = v2;

        int ibase = i * 6;
        int vbase = i * 4;
        indices[ibase + 0] = vbase + 0;
        indices[ibase + 1] = vbase + 1;
        indices[ibase + 2] = vbase + 2;
        indices[ibase + 3] = vbase + 2;
        indices[ibase + 4] = vbase + 3;
        indices[ibase + 5] = vbase + 0;
    }

    VAO vao;
    VBO vbo;
    EBO ebo;

    vao_create(&vao);
    vao_bind(&vao);

    vbo_create(&vbo, vertices, vertex_count * 4 * sizeof(float));
    ebo_create(&ebo, indices, index_count * sizeof(int));

    vbo_attr(0, 2, 4 * sizeof(float), 0);
    vbo_attr(1, 2, 4 * sizeof(float), 2);

    text->gpu_data.vbo = vbo;
    text->gpu_data.vao = vao;
    text->gpu_data.ebo = ebo;
    text->index_count = index_count;

    free(vertices);
    free(indices);
}

void text_draw(HText* text) {
    if (text->index_count == 0) return;

    program_use(&text_program);

    mat4 projection;
    glm_ortho(0.0f, (float)_win->width, (float)_win->height, 0.0f, -1.0f, 1.0f, projection);

    program_set_mat4(&text_program, "projection", (float*)projection);

    texture_bind(&text_atlas, 0);
    program_set_int(&text_program, "tex_atlas", 0);
    program_set_uint(&text_program, "color_data", (uint32_t)text->color);

    render_count(&text->gpu_data.vao, text->index_count);
}

void text_free(HText* text) {
    vao_free(&text->gpu_data.vao);
    vbo_free(&text->gpu_data.vbo);
    ebo_free(&text->gpu_data.ebo);
    text->index_count = 0;
}