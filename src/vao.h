#ifndef VAO_H
#define VAO_H

typedef struct {
    unsigned int id;
} VAO;

void vao_create(VAO* vao);
void vao_bind(VAO* vao);
void render(VAO* vao);

void vao_free(VAO* vao);

#endif