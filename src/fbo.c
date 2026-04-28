#include "fbo.h"
#include "glad.h"

#include <stdio.h>

void fbo_create(FBO* fbo, int width, int height, int color_count)
{
    glGenFramebuffers(1, &fbo->id);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo->id);

    fbo->color_count = color_count;

    for (int i = 0; i < color_count; i++)
    {
        glGenTextures(1, &fbo->color_attachments[i]);
        glBindTexture(GL_TEXTURE_2D, fbo->color_attachments[i]);

        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGB16F,
            width,
            height,
            0,
            GL_RGB,
            GL_FLOAT,
            NULL
        );

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glFramebufferTexture2D(
            GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0 + i,
            GL_TEXTURE_2D,
            fbo->color_attachments[i],
            0
        );
    }

    // ---- DEPTH BUFFER ----
    glGenRenderbuffers(1, &fbo->depth_attachment);
    glBindRenderbuffer(GL_RENDERBUFFER, fbo->depth_attachment);

    glRenderbufferStorage(
        GL_RENDERBUFFER,
        GL_DEPTH_COMPONENT24,
        width,
        height
    );

    glFramebufferRenderbuffer(
        GL_FRAMEBUFFER,
        GL_DEPTH_ATTACHMENT,
        GL_RENDERBUFFER,
        fbo->depth_attachment
    );

    unsigned int attachments[MAX_FBO_ATTACHMENTS];
    for (int i = 0; i < color_count; i++)
    {
        attachments[i] = GL_COLOR_ATTACHMENT0 + i;
    }

    glDrawBuffers(color_count, attachments);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        printf("FBO ERROR: Framebuffer not complete!\n");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void fbo_bind(FBO* fbo)
{
    glBindFramebuffer(GL_FRAMEBUFFER, fbo->id);
}

void fbo_unbind()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void fbo_bind_texture(FBO* fbo, int attachment_index, unsigned int unit)
{
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, fbo->color_attachments[attachment_index]);
}

void fbo_free(FBO* fbo)
{
    if (!fbo || fbo->id == 0)
        return;

    glDeleteFramebuffers(1, &fbo->id);

    for (int i = 0; i < fbo->color_count; i++)
    {
        glDeleteTextures(1, &fbo->color_attachments[i]);
    }

    glDeleteRenderbuffers(1, &fbo->depth_attachment);

    fbo->id = 0;
    fbo->color_count = 0;
}