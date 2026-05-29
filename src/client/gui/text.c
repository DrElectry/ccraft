#include "gui/text.h"
#include "core/gfx.h"
#include "gl/vao.h"
#include "gl/vbo.h"
#include "gl/ebo.h"
#include "gl/tex.h"
#include "utils/fm.h"
#include "core/win.h"
#include "gl/shader.h"
#include "utils/log.h"

#include <cglm/cglm.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

static const char CHARSET[] =
"ABCDEFGHIJKLMNOPQRSTUVWXYZ!/<>:+1234567890&=().? ";

static int char_index(unsigned char c) {
    for (int i = 0; CHARSET[i] != '\0'; i++) {
        if (CHARSET[i] == c)
            return i;
    }
    return -1;
}

static Texture text_atlas;
static Program text_program;
static VAO global_vao;
static VBO global_vbo;
static EBO global_ebo;
static bool gpu_resources_initialized = false;

typedef struct TextCache {
    HText* owner;
    uint16_t color;
    int x, y;
    int glyph_count;
    int vertex_count;
    int index_count;
    float* vertices;
    bool dirty;
} TextCache;

static TextCache text_cache[MAX_TEXTS];
static int active_texts = 0;

static TextCache* get_cache(HText* text) {
    for (int i = 0; i < active_texts; i++) {
        if (text_cache[i].owner == text) {
            return &text_cache[i];
        }
    }
    return NULL;
}

static TextCache* allocate_cache(HText* text) {
    if (active_texts >= MAX_TEXTS) {
        return NULL;
    }
    TextCache* cache = &text_cache[active_texts++];
    memset(cache, 0, sizeof(TextCache));
    cache->owner = text;
    cache->dirty = true;
    return cache;
}

static void free_cache(TextCache* cache) {
    if (cache->owner) {
        free(cache->vertices);
        cache->vertices = NULL;
        cache->owner = NULL;
        cache->glyph_count = 0;
        cache->vertex_count = 0;
        cache->index_count = 0;
        cache->dirty = false;
    }
}

static void rebuild_text_geometry(TextCache* cache, const char* str, uint16_t color, int x, int y) {
    int len = (int)strlen(str);
    if (len == 0) {
        cache->glyph_count = 0;
        cache->vertex_count = 0;
        cache->index_count = 0;
        free(cache->vertices);
        cache->vertices = NULL;
        cache->dirty = false;
        return;
    }

    int vertex_count = len * 4;
    int index_count = len * 6;

    float* vertices = realloc(cache->vertices, vertex_count * 4 * sizeof(float));
    if (!vertices) {
        return;
    }
    cache->vertices = vertices;

    for (int i = 0; i < len; i++) {
        unsigned char c = (unsigned char)str[i];
        int idx = char_index(c);
        if (idx == -1) idx = 0;

        int grid_x = idx % ATLAS_COLS;
        int grid_y = idx / ATLAS_COLS;
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
    }

    cache->glyph_count = len;
    cache->vertex_count = vertex_count;
    cache->index_count = index_count;
    cache->color = color;
    cache->x = x;
    cache->y = y;
    cache->dirty = true;
}

static void ensure_global_resources(void) {
    if (gpu_resources_initialized) return;

    vao_create(&global_vao);
    vao_bind(&global_vao);

    vbo_create(&global_vbo, NULL, 0);
    ebo_create(&global_ebo, NULL, 0);

    vbo_attr(0, 2, 4 * sizeof(float), 0);
    vbo_attr(1, 2, 4 * sizeof(float), 2);

    gpu_resources_initialized = true;
}

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

    ensure_global_resources();
}

void text_create(HText* text, char* string, uint16_t color, int x, int y) {
    if (!text) return;

    TextCache* cache = get_cache(text);
    if (cache) {
        cache->dirty = true;
        rebuild_text_geometry(cache, string, color, x, y);
        text->data = string;
        text->color = (uint8_t)(color & 0xFF);
        text->x = x;
        text->y = y;
        text->index_count = cache->index_count;
        return;
    }

    cache = allocate_cache(text);
    if (!cache) return;

    text->data = string;
    text->color = (uint8_t)(color & 0xFF);
    text->x = x;
    text->y = y;
    text->index_count = 0;

    rebuild_text_geometry(cache, string, color, x, y);
    text->index_count = cache->index_count;
}

void text_draw(HText* text) {
    if (!text || text->index_count == 0) return;

    TextCache* cache = get_cache(text);
    if (!cache) return;

    if (cache->dirty) {
        vao_bind(&global_vao);

        size_t vertex_bytes = cache->vertex_count * 4 * sizeof(float);
        glBindBuffer(GL_ARRAY_BUFFER, global_vbo.id);
        GLint current_size = 0;
        glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &current_size);
        if ((GLsizeiptr)vertex_bytes > current_size) {
            glBufferData(GL_ARRAY_BUFFER, vertex_bytes, NULL, GL_DYNAMIC_DRAW);
        }
        glBufferSubData(GL_ARRAY_BUFFER, 0, vertex_bytes, cache->vertices);

        int needed_indices = cache->index_count;
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, global_ebo.id);
        glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &current_size);

        int* indices = malloc(needed_indices * sizeof(int));
        if (indices) {
            for (int i = 0; i < cache->glyph_count; i++) {
                int base = i * 6;
                int vbase = i * 4;
                indices[base + 0] = vbase + 0;
                indices[base + 1] = vbase + 1;
                indices[base + 2] = vbase + 2;
                indices[base + 3] = vbase + 2;
                indices[base + 4] = vbase + 3;
                indices[base + 5] = vbase + 0;
            }
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, needed_indices * sizeof(int), indices, GL_STATIC_DRAW);
            free(indices);
        }

        cache->dirty = false;
    }

    program_use(&text_program);

    mat4 projection;
    glm_ortho(0.0f, (float)_win->width, (float)_win->height, 0.0f, -1.0f, 1.0f, projection);

    program_set_mat4(&text_program, "projection", (float*)projection);

    texture_bind(&text_atlas, 0);
    program_set_int(&text_program, "tex_atlas", 0);

    // shadow pass (0x08)

    if (cache->vertex_count > 0) {
        float* tmp_vertices = malloc(cache->vertex_count * 4 * sizeof(float));
        if (tmp_vertices) {
            memcpy(tmp_vertices, cache->vertices, cache->vertex_count * 4 * sizeof(float));
            for (int i = 0; i < cache->vertex_count; i++) {
                tmp_vertices[i * 4 + 0] += 2.0f;
                tmp_vertices[i * 4 + 1] += 2.0f;
            }

            size_t vertex_bytes = cache->vertex_count * 4 * sizeof(float);
            glBindBuffer(GL_ARRAY_BUFFER, global_vbo.id);
            glBufferSubData(GL_ARRAY_BUFFER, 0, vertex_bytes, tmp_vertices);

            program_set_uint(&text_program, "color_data", (uint32_t)0x08u);
            vao_bind(&global_vao);
            glDrawElements(GL_TRIANGLES, text->index_count, GL_UNSIGNED_INT, NULL);

            glBufferSubData(GL_ARRAY_BUFFER, 0, vertex_bytes, cache->vertices);
            free(tmp_vertices);
        }
    }

    program_set_uint(&text_program, "color_data", (uint32_t)text->color);


    vao_bind(&global_vao);
    glDrawElements(GL_TRIANGLES, text->index_count, GL_UNSIGNED_INT, NULL);
}


void text_free(HText* text) {
    if (!text) return;
    TextCache* cache = get_cache(text);
    if (cache) {
        free_cache(cache);
        for (int i = 0; i < active_texts; i++) {
            if (&text_cache[i] == cache) {
                memmove(&text_cache[i], &text_cache[i+1], (active_texts - i - 1) * sizeof(TextCache));
                active_texts--;
                break;
            }
        }
    }
    text->index_count = 0;
}
