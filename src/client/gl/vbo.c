#include "gl/vbo.h"
#include "utils/log.h"
#include "glad.h"
#include <stdint.h>

void vbo_create(VBO* vbo, const void* vertices, size_t size) {
    glGenBuffers(1, &vbo->id);

    glBindBuffer(GL_ARRAY_BUFFER, vbo->id);
    glBufferData(GL_ARRAY_BUFFER, size, vertices, GL_STATIC_DRAW);
}

void vbo_attr(int layout, int size, int stride, int offset) {
    glVertexAttribPointer(
        layout,
        size,
        GL_FLOAT,
        GL_FALSE,
        stride,
        (void*)(offset * sizeof(float))
    );
    glEnableVertexAttribArray(layout);
}

void vbo_attr_i(int layout, int size, int stride, int offset) {
    glVertexAttribIPointer(
        layout,
        size,
        GL_INT,
        stride,
        (void*)(offset * sizeof(float))
    );
    glEnableVertexAttribArray(layout);
}

void vbo_attr_iu(int layout, int components, int stride_bytes, int offset_bytes, unsigned int divisor) {
    glVertexAttribIPointer(
        layout,
        components,
        GL_UNSIGNED_INT,
        stride_bytes,
        (void*)(intptr_t)offset_bytes
    );
    glEnableVertexAttribArray(layout);
    glVertexAttribDivisor(layout, divisor);
}

void vbo_free(VBO* vbo) {
    if (vbo && vbo->id != 0) {
        glDeleteBuffers(1, &vbo->id);
        vbo->id = 0;
    }
}

