#ifndef VBO_H
#define VBO_H

#include <stddef.h>

typedef struct {
    unsigned int id;
} VBO;

void vbo_create(VBO* vbo, const void* vertices, size_t size);
void vbo_attr(int layout, int size, int stride, int offset); // !!! CALL THIS EXACTLY AFTER VBO_CREATE !!!
void vbo_attr_i(int layout, int size, int stride, int offset);
void vbo_attr_iu(int layout, int components, int stride_bytes, int offset_bytes, unsigned int divisor);
void vbo_free(VBO* vbo);


#endif