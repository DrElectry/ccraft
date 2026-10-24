#include "gl/tex.h"
#define STB_IMAGE_IMPLEMENTATION // tripling the compile times
#include "stb_image.h"
#include "utils/log.h"

void texture_create(Texture* tex, char* src) {
    unsigned char *data = stbi_load(src, &tex->width, &tex->height, &tex->channels, 0);
    ASSERT(data, "Failed to load texture: %s", src);

    glGenTextures(1, &tex->id);
    glBindTexture(GL_TEXTURE_2D, tex->id);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, tex->wrap_s);	
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, tex->wrap_t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, tex->min_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, tex->mag_filter);

    GLenum format = (tex->channels == 4) ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, format, tex->width, tex->height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(data);
}

static GLenum tex_format(int channels) {
    if (channels == 1) return GL_RED;
    if (channels == 4) return GL_RGBA;
    return GL_RGB;
}

static GLenum tex_internal(int channels) {
    if (channels == 1) return GL_R8;
    if (channels == 4) return GL_RGBA;
    return GL_RGB;
}

void texture_create_raw(Texture* tex, unsigned char* data, int w, int h, int channels) {
    tex->width = w;
    tex->height = h;
    tex->channels = channels;

    glGenTextures(1, &tex->id);
    glBindTexture(GL_TEXTURE_2D, tex->id);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, tex->wrap_s);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, tex->wrap_t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, tex->min_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, tex->mag_filter);

    GLint unpack = 4;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpack);
    if (channels == 1) glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    GLenum fmt = tex_format(channels);
    glTexImage2D(GL_TEXTURE_2D, 0, (GLint)tex_internal(channels), w, h, 0, fmt, GL_UNSIGNED_BYTE, data);

    if (channels == 1) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_RED);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_RED);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_RED);
    }

    glPixelStorei(GL_UNPACK_ALIGNMENT, unpack);
}

void texture_update(Texture* tex, unsigned char* data) {
    glBindTexture(GL_TEXTURE_2D, tex->id);

    GLint unpack = 4;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpack);
    if (tex->channels == 1) glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    GLenum fmt = tex_format(tex->channels);
    glTexImage2D(GL_TEXTURE_2D, 0, (GLint)tex_internal(tex->channels), tex->width, tex->height, 0, fmt, GL_UNSIGNED_BYTE, data);

    glPixelStorei(GL_UNPACK_ALIGNMENT, unpack);
}

void texture_free(Texture* tex) {
    if (!tex || tex->id == 0) return;
    glDeleteTextures(1, &tex->id);
    tex->id = 0;
}

void texture_bind(Texture* tex, int unit) {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, tex->id);
}