#include "gui/text.h"
#include "core/gfx.h"
#include "gl/vao.h"
#include "gl/vbo.h"
#include "gl/ebo.h"
#include "gl/tex.h"
#include "utils/file.h"
#include "core/win.h"
#include "gl/shader.h"
#include "utils/log.h"
#include "font.h"

#include <cglm/cglm.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

#define BAKE_FIRST 32
#define BAKE_COUNT 224

static Program text_program;
static VAO global_vao;
static VBO global_vbo;
static EBO global_ebo;
static bool gpu_resources_initialized = false;
static int ebo_capacity = 0;

typedef struct {
    char name[32];
    uint32_t size;
    int used;
    int pw, ph;
    int line_height;
    float ascent;
    float descent;
    float line_gap;
    t_baked_glyph glyphs[BAKE_COUNT];
    Texture atlas;
    void* font;
} FontAtlas;

static FontAtlas atlases[MAX_FONT_ATLASES];

typedef struct TextCache {
    HText* owner;
    uint16_t color;
    int x, y;
    uint32_t size;
    char font[32];
    int glyph_count;
    int vertex_count;
    int index_count;
    float* vertices;
    FontAtlas* atlas;
    bool dirty;
} TextCache;

static TextCache text_cache[MAX_TEXTS];
static int active_texts = 0;

static void atlas_free(FontAtlas* a) {
    if (!a->used) return;
    texture_free(&a->atlas);
    memset(a, 0, sizeof(*a));
}

static int atlas_bake(FontAtlas* a, void* font, const char* name, uint32_t size) {
    int pw = 512;
    int ph = 512;
    if (size > 32) {
        pw = 1024;
        ph = 1024;
    }
    if (size > 64) {
        pw = 2048;
        ph = 2048;
    }

    unsigned char* pixels = NULL;
    int ok = 0;

    for (int attempt = 0; attempt < 3; attempt++) {
        pixels = (unsigned char*)malloc((size_t)pw * (size_t)ph);
        if (!pixels) return 0;

        ok = font_bake(font, (float)size, pixels, pw, ph, BAKE_FIRST, BAKE_COUNT, a->glyphs);
        if (ok) break;

        free(pixels);
        pixels = NULL;
        pw *= 2;
        ph *= 2;
    }

    if (!ok || !pixels) {
        free(pixels);
        return 0;
    }

    if (a->atlas.id) texture_free(&a->atlas);

    a->atlas.wrap_s = GL_CLAMP_TO_EDGE;
    a->atlas.wrap_t = GL_CLAMP_TO_EDGE;
    a->atlas.min_filter = GL_LINEAR;
    a->atlas.mag_filter = GL_LINEAR;
    texture_create_raw(&a->atlas, pixels, pw, ph, 1);
    free(pixels);

    float ascent = 0, descent = 0, gap = 0;
    font_vmetrics(font, (float)size, &ascent, &descent, &gap);

    strncpy(a->name, name, sizeof(a->name) - 1);
    a->size = size;
    a->pw = pw;
    a->ph = ph;
    a->ascent = ascent;
    a->descent = descent;
    a->line_gap = gap;
    a->line_height = (int)(ascent - descent + gap + 0.5f);
    if (a->line_height < 1) a->line_height = (int)size;
    a->font = font;
    a->used = 1;
    return 1;
}

static FontAtlas* atlas_get(const char* name, uint32_t size) {
    if (!name || name[0] == '\0') name = FONT_DEFAULT;
    if (size == 0) size = FONT_DEFAULT_SIZE;

    for (int i = 0; i < MAX_FONT_ATLASES; i++) {
        if (atlases[i].used && atlases[i].size == size && strcmp(atlases[i].name, name) == 0)
            return &atlases[i];
    }

    void* font = get_font(name);
    if (!font) {
        font = get_font(FONT_DEFAULT);
        name = FONT_DEFAULT;
        if (!font) return NULL;
    }

    FontAtlas* slot = NULL;
    for (int i = 0; i < MAX_FONT_ATLASES; i++) {
        if (!atlases[i].used) {
            slot = &atlases[i];
            break;
        }
    }
    if (!slot) return NULL;

    if (!atlas_bake(slot, font, name, size))
        return NULL;
    return slot;
}

static void mark_font_dirty(const char* name, uint32_t size) {
    for (int i = 0; i < active_texts; i++) {
        if (strcmp(text_cache[i].font, name) != 0) continue;
        if (size != 0 && text_cache[i].size != size) continue;
        text_cache[i].dirty = true;
    }
}

int font_load(const char* name, const char* path) {
    if (!name || !path) return 0;
    if (!create_new_font(path, (char*)name, "regular"))
        return 0;
    atlas_get(name, FONT_DEFAULT_SIZE);
    return 1;
}

int font_reload(const char* name) {
    const char* path = get_font_path(name);
    if (!path) return 0;

    char path_copy[256];
    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';

    uint32_t sizes[MAX_FONT_ATLASES];
    int n = 0;
    for (int i = 0; i < MAX_FONT_ATLASES; i++) {
        if (atlases[i].used && strcmp(atlases[i].name, name) == 0)
            sizes[n++] = atlases[i].size;
    }

    font_unload(name);
    if (!create_new_font(path_copy, (char*)name, "regular"))
        return 0;

    if (n == 0) sizes[n++] = FONT_DEFAULT_SIZE;
    for (int i = 0; i < n; i++)
        atlas_get(name, sizes[i]);

    mark_font_dirty(name, 0);
    return 1;
}

void font_unload(const char* name) {
    if (!name) return;
    for (int i = 0; i < MAX_FONT_ATLASES; i++) {
        if (atlases[i].used && strcmp(atlases[i].name, name) == 0)
            atlas_free(&atlases[i]);
    }
    delete_font(name);
}

void font_rebuild(const char* name, uint32_t size) {
    if (!name) return;
    void* font = get_font(name);
    if (!font) return;

    if (size == 0) {
        for (int i = 0; i < MAX_FONT_ATLASES; i++) {
            if (atlases[i].used && strcmp(atlases[i].name, name) == 0)
                atlas_bake(&atlases[i], font, name, atlases[i].size);
        }
        mark_font_dirty(name, 0);
        return;
    }

    FontAtlas* a = NULL;
    for (int i = 0; i < MAX_FONT_ATLASES; i++) {
        if (atlases[i].used && atlases[i].size == size && strcmp(atlases[i].name, name) == 0) {
            a = &atlases[i];
            break;
        }
    }
    if (!a) {
        atlas_get(name, size);
        return;
    }
    atlas_bake(a, font, name, size);
    mark_font_dirty(name, size);
}

static TextCache* get_cache(HText* text) {
    for (int i = 0; i < active_texts; i++) {
        if (text_cache[i].owner == text)
            return &text_cache[i];
    }
    return NULL;
}

static TextCache* allocate_cache(HText* text) {
    if (active_texts >= MAX_TEXTS) return NULL;
    TextCache* cache = &text_cache[active_texts++];
    memset(cache, 0, sizeof(TextCache));
    cache->owner = text;
    cache->dirty = true;
    return cache;
}

static void free_cache(TextCache* cache) {
    if (!cache->owner) return;
    free(cache->vertices);
    cache->vertices = NULL;
    cache->owner = NULL;
    cache->glyph_count = 0;
    cache->vertex_count = 0;
    cache->index_count = 0;
    cache->dirty = false;
}

static int next_codepoint(const char** s) {
    const unsigned char* p = (const unsigned char*)*s;
    if (*p == 0) return 0;
    if (*p < 0x80) {
        *s += 1;
        return *p;
    }
    if ((p[0] & 0xE0) == 0xC0 && p[1]) {
        int cp = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F);
        *s += 2;
        return cp;
    }
    *s += 1;
    return *p;
}

static int glyph_slot(int cp) {
    if (cp < BAKE_FIRST || cp >= BAKE_FIRST + BAKE_COUNT)
        cp = '?';
    return cp - BAKE_FIRST;
}

static void emit_quad(float* vertices, int i, t_glyph_quad* q) {
    int base = i * 16;
    vertices[base + 0] = q->x0;
    vertices[base + 1] = q->y0;
    vertices[base + 2] = q->s0;
    vertices[base + 3] = q->t0;

    vertices[base + 4] = q->x0;
    vertices[base + 5] = q->y1;
    vertices[base + 6] = q->s0;
    vertices[base + 7] = q->t1;

    vertices[base + 8] = q->x1;
    vertices[base + 9] = q->y1;
    vertices[base + 10] = q->s1;
    vertices[base + 11] = q->t1;

    vertices[base + 12] = q->x1;
    vertices[base + 13] = q->y0;
    vertices[base + 14] = q->s1;
    vertices[base + 15] = q->t0;
}

static void rebuild_text_geometry(TextCache* cache, HText* text, const char* str) {
    FontAtlas* atlas = atlas_get(text->font, text->size);
    cache->atlas = atlas;

    if (!atlas || !str || str[0] == '\0') {
        cache->glyph_count = 0;
        cache->vertex_count = 0;
        cache->index_count = 0;
        cache->dirty = false;
        return;
    }

    int count = 0;
    const char* s = str;
    while (next_codepoint(&s)) count++;
    if (count > MAX_GLYPHS_PER_TEXT) count = MAX_GLYPHS_PER_TEXT;
    if (count == 0) {
        cache->glyph_count = 0;
        cache->vertex_count = 0;
        cache->index_count = 0;
        cache->dirty = false;
        return;
    }

    int vertex_count = count * 4;
    float* vertices = realloc(cache->vertices, (size_t)vertex_count * 4 * sizeof(float));
    if (!vertices) return;
    cache->vertices = vertices;

    float xpos = (float)text->x;
    float ypos = (float)text->y + atlas->ascent;
    int prev = 0;
    int gi = 0;

    s = str;
    for (;;) {
        int cp = next_codepoint(&s);
        if (!cp) break;
        if (gi >= count) break;
        if (cp == '\n' || cp == '\r') {
            prev = 0;
            continue;
        }

        if (prev)
            xpos += font_kern(atlas->font, prev, cp, (float)atlas->size);

        t_glyph_quad q;
        font_quad(atlas->glyphs, atlas->pw, atlas->ph, glyph_slot(cp), &xpos, &ypos, &q, 1);
        emit_quad(vertices, gi, &q);
        prev = cp;
        gi++;
    }

    cache->glyph_count = gi;
    cache->vertex_count = gi * 4;
    cache->index_count = gi * 6;
    cache->color = text->color;
    cache->x = text->x;
    cache->y = text->y;
    cache->size = text->size;
    strncpy(cache->font, text->font, sizeof(cache->font) - 1);
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

static void ensure_indices(int glyph_count) {
    int needed = glyph_count * 6;
    if (needed <= ebo_capacity) return;

    int* indices = malloc((size_t)needed * sizeof(int));
    if (!indices) return;

    for (int i = 0; i < glyph_count; i++) {
        int base = i * 6;
        int vbase = i * 4;
        indices[base + 0] = vbase + 0;
        indices[base + 1] = vbase + 1;
        indices[base + 2] = vbase + 2;
        indices[base + 3] = vbase + 2;
        indices[base + 4] = vbase + 3;
        indices[base + 5] = vbase + 0;
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, global_ebo.id);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, needed * (int)sizeof(int), indices, GL_STATIC_DRAW);
    free(indices);
    ebo_capacity = needed;
}

void text_init(const char* vsrc, const char* fsrc) {
    File v = file_open(vsrc);
    File f = file_open(fsrc);

    Shader a, b;
    a.type = GL_VERTEX_SHADER;
    b.type = GL_FRAGMENT_SHADER;

    shader_create(&a, v.data);
    shader_create(&b, f.data);
    program_create(&text_program, &a, &b);

    ensure_global_resources();

    if (!get_font(FONT_DEFAULT))
        ASSERT(font_load(FONT_DEFAULT, FONT_DEFAULT_PATH), "failed to load %s", FONT_DEFAULT_PATH);
}

void text_destroy(void) {
    for (int i = 0; i < active_texts; i++)
        free_cache(&text_cache[i]);
    active_texts = 0;

    for (int i = 0; i < MAX_FONT_ATLASES; i++) {
        if (!atlases[i].used) continue;
        char name[32];
        strncpy(name, atlases[i].name, sizeof(name) - 1);
        font_unload(name);
    }
}

static void ensure_text_defaults(HText* text) {
    if (text->font[0] == '\0')
        strncpy(text->font, FONT_DEFAULT, sizeof(text->font) - 1);
    if (text->size == 0)
        text->size = FONT_DEFAULT_SIZE;
}

void text_set_font(HText* text, const char* font_name) {
    if (!text) return;
    strncpy(text->font, font_name ? font_name : FONT_DEFAULT, sizeof(text->font) - 1);
    text->font[sizeof(text->font) - 1] = '\0';
    TextCache* cache = get_cache(text);
    if (cache && text->data)
        rebuild_text_geometry(cache, text, text->data);
}

void text_set_size(HText* text, uint32_t size) {
    if (!text) return;
    text->size = size ? size : FONT_DEFAULT_SIZE;
    TextCache* cache = get_cache(text);
    if (cache && text->data)
        rebuild_text_geometry(cache, text, text->data);
}

void text_create(HText* text, char* string, uint8_t color, float alpha, int x, int y) {
    if (!text) return;

    ensure_text_defaults(text);

    TextCache* cache = get_cache(text);
    if (!cache) {
        cache = allocate_cache(text);
        if (!cache) return;
    }

    text->data = string;
    text->color = color;
    text->alpha = alpha;
    text->x = x;
    text->y = y;

    rebuild_text_geometry(cache, text, string ? string : "");
    text->index_count = cache->index_count;
}

int text_measure_width(HText* text) {
    if (!text || !text->data) return 0;
    ensure_text_defaults(text);
    void* font = get_font(text->font[0] ? text->font : FONT_DEFAULT);
    if (!font) font = get_font(FONT_DEFAULT);
    if (!font) return 0;
    return (int)text_width(font, text->data, text->size);
}

int text_measure_height(HText* text) {
    if (!text) return FONT_DEFAULT_SIZE;
    ensure_text_defaults(text);
    FontAtlas* a = atlas_get(text->font, text->size);
    return a ? a->line_height : (int)text->size;
}

int text_default_line_height(void) {
    FontAtlas* a = atlas_get(FONT_DEFAULT, FONT_DEFAULT_SIZE);
    return a ? a->line_height : FONT_DEFAULT_SIZE;
}

void text_draw(HText* text) {
    if (!text || text->index_count == 0) return;

    TextCache* cache = get_cache(text);
    if (!cache || !cache->atlas) return;

    if (cache->dirty) {
        vao_bind(&global_vao);

        size_t vertex_bytes = (size_t)cache->vertex_count * 4 * sizeof(float);
        glBindBuffer(GL_ARRAY_BUFFER, global_vbo.id);
        GLint current_size = 0;
        glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &current_size);
        if ((GLsizeiptr)vertex_bytes > current_size)
            glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)vertex_bytes, NULL, GL_DYNAMIC_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)vertex_bytes, cache->vertices);

        ensure_indices(cache->glyph_count);
        cache->dirty = false;
    }

    program_use(&text_program);

    mat4 projection;
    glm_ortho(0.0f, (float)_win->width, (float)_win->height, 0.0f, -1.0f, 1.0f, projection);
    program_set_mat4(&text_program, "projection", (float*)projection);

    texture_bind(&cache->atlas->atlas, 0);
    program_set_int(&text_program, "tex_atlas", 0);

    GLboolean depth_test_enabled = glIsEnabled(GL_DEPTH_TEST);
    GLboolean depth_mask;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_mask);

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    vao_bind(&global_vao);

    program_set_float(&text_program, "alpha", text->alpha);
    program_set_vec2(&text_program, "offset", (vec2){2.0f, 2.0f});
    program_set_uint(&text_program, "color_data", 0x08u);
    glDrawElements(GL_TRIANGLES, text->index_count, GL_UNSIGNED_INT, NULL);

    program_set_vec2(&text_program, "offset", (vec2){0.0f, 0.0f});
    program_set_uint(&text_program, "color_data", (uint32_t)text->color);
    glDrawElements(GL_TRIANGLES, text->index_count, GL_UNSIGNED_INT, NULL);

    if (depth_test_enabled) glEnable(GL_DEPTH_TEST);
    glDepthMask(depth_mask);
}

void text_free(HText* text) {
    if (!text) return;
    TextCache* cache = get_cache(text);
    if (cache) {
        free_cache(cache);
        for (int i = 0; i < active_texts; i++) {
            if (&text_cache[i] == cache) {
                memmove(&text_cache[i], &text_cache[i + 1], (size_t)(active_texts - i - 1) * sizeof(TextCache));
                active_texts--;
                break;
            }
        }
    }
    text->index_count = 0;
}
