#ifndef VBO_H
#define VBO_H

#include <stddef.h>

typedef struct {
    unsigned int id;
} VBO;

void vbo_create(VBO* vbo, float* vertices);
void vbo_attr(int layout, int size, int stride, int offset); // !!! CALL THIS EXACTLY AFTER VBO_CREATE !!!

#endif