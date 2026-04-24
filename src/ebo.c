#include "ebo.h"
#include "glad.h"
#include <stdlib.h>

#include <stddef.h>

void ebo_create(EBO* ebo, int* indices, size_t size) {
  glGenBuffers(1, &ebo->id);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo->id);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, indices, GL_STATIC_DRAW);
}
