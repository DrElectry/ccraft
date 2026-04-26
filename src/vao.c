#include "vao.h"
#include "glad.h"
#include <stddef.h>

void vao_create(VAO* vao) {
    glGenVertexArrays(1, &vao->id);
}

void vao_bind(VAO* vao) {
    glBindVertexArray(vao->id);
}

void render(VAO* vao) {
    vao_bind(vao);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, NULL);
}

void vao_free(VAO* vao) {
    if (vao && vao->id != 0) {
        glDeleteVertexArrays(1, &vao->id);
        vao->id = 0;
    }
}