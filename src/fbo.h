#ifndef FBO_H
#define FBO_H

#include <stddef.h>

#define MAX_FBO_ATTACHMENTS 8

typedef enum {
    FBO_COLOR_RGB16F,
    FBO_COLOR_RG16F,
    FBO_COLOR_RGBA16F,
} FBOColorFormat;

typedef struct {
    unsigned int id;

    unsigned int color_attachments[MAX_FBO_ATTACHMENTS];
    int color_count;
    FBOColorFormat color_formats[MAX_FBO_ATTACHMENTS];

    unsigned int depth_attachment;
} FBO;

void fbo_create(FBO* fbo, int width, int height, int color_count);
void fbo_bind(FBO* fbo);
void fbo_unbind();

void fbo_bind_texture(FBO* fbo, int attachment_index, unsigned int unit);
void fbo_bind_depth_texture(FBO* fbo, unsigned int unit);
void fbo_create_depth(FBO* fbo, int width, int height);

void fbo_free(FBO* fbo);

#endif
