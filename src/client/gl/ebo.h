#ifndef EBO_H
#define EBO_H

#include <stddef.h>

typedef struct {
    unsigned int id;
} EBO;

void ebo_create(EBO* ebo, int* indices, size_t size); // !!! CALL THIS BEFORE VBO_ATTR !!!
void ebo_free(EBO* ebo);

#endif