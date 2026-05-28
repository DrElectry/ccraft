#ifndef TEX_H
#define TEX_H

#include "glad.h"

typedef struct
{
    unsigned int id;
    int width, height, channels;

    GLenum wrap_s;
    GLenum wrap_t;
    GLenum min_filter;
    GLenum mag_filter;
    GLenum internal_format;
    GLenum format;
} Texture;

void texture_create(Texture* tex, char* src);
void texture_bind(Texture* tex, int unit);

#endif