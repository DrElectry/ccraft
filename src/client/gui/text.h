#ifndef TEXT_H
#define TEXT_H

#include "core/gfx.h"

#include <stdint.h>

#define FONT_DEFAULT "Arial"
#define FONT_DEFAULT_SIZE 16
#define FONT_DEFAULT_PATH "assets/fonts/Arial.ttf"

#define MAX_GLYPHS_PER_TEXT 256
#define MAX_TEXTS 256
#define MAX_FONT_ATLASES 16

#define CHAR_WIDTH 16
#define CHAR_HEIGHT 16
#define ATLAS_COLS 16
#define ATLAS_ROWS 16

typedef struct {
    char* data;
    uint8_t color; // 4 bits bg 4 bits fg
    float alpha;

    int x, y;
    int index_count;

    uint32_t size;
    char font[32];

    GPUBuffer gpu_data;
} HText; // HUD TEXT

void text_init(const char* vsrc, const char* fsrc);
void text_destroy(void);

void text_create(HText* text, char* string, uint8_t color, float alpha, int x, int y);
void text_set_font(HText* text, const char* font_name);
void text_set_size(HText* text, uint32_t size);
void text_draw(HText* text);
void text_free(HText* text);

int text_measure_width(HText* text);
int text_measure_height(HText* text);
int text_default_line_height(void);

int font_load(const char* name, const char* path);
int font_reload(const char* name);
void font_unload(const char* name);
void font_rebuild(const char* name, uint32_t size);

#endif
