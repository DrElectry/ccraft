#ifndef TEXT_H
#define TEXT_H

#include "gfx.h"

#include <stdint.h>

typedef struct {
    char* data;
    uint16_t color; // 4 bits color 4 bits foreground

    int x, y;
    int index_count;

    GPUBuffer gpu_data;
} HText; // HUD TEXT

void text_init(const char* vsrc, const char* fsrc, const char* asrc);
void text_create(HText* text, char* string, uint16_t color, int x, int y);
void text_draw(HText* text);
void text_free(HText* text);

#endif
