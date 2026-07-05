#ifndef TEXT_H
#define TEXT_H

#include "core/gfx.h"

#include <stdint.h>

#define CHAR_WIDTH 16
#define CHAR_HEIGHT 16
#define ATLAS_COLS 16
#define ATLAS_ROWS 16
#define MAX_GLYPHS_PER_TEXT 256
#define MAX_TEXTS 256

typedef struct {
    char* data;
    uint8_t color; // 4 bits color 4 bits foreground
    float alpha;   // transparency

    int x, y;
    int index_count;

    GPUBuffer gpu_data;
} HText; // HUD TEXT

void text_init(const char* vsrc, const char* fsrc, const char* asrc);
void text_create(HText* text, char* string, uint8_t color, float alpha, int x, int y);
void text_draw(HText* text);
void text_free(HText* text);

#endif