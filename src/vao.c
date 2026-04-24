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
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, NULL);
}
