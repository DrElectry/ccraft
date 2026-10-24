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
void texture_create_raw(Texture* tex, unsigned char* data, int w, int h, int channels);
void texture_update(Texture* tex, unsigned char* data);
void texture_free(Texture* tex);
void texture_bind(Texture* tex, int unit);

#endif