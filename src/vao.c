#include "vao.h"
#include "glad.h"

void vao_create(VAO* vao) {
    glGenVertexArrays(1, &vao->id);
}

void vao_bind(VAO* vao) {
    glBindVertexArray(vao->id);
}

void render(VAO* vao) {
    vao_bind(vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}