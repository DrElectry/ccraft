#include "vbo.h"
#include "log.h"
#include "glad.h"

void vbo_create(VBO* vbo, float* vertices, size_t size) {
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