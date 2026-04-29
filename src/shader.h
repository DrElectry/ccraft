#ifndef SHADER_H
#define SHADER_H

#include "glad.h"

typedef struct {
    unsigned int id;
    GLenum type; // GL_VERTEX_SHADER GL_FRAGMENT_SHADER
} Shader;

typedef struct {
    unsigned int id;
    Shader* vertex;
    Shader* fragment;
} Program;

void shader_create(Shader* packet, char* src);
void program_create(Program* packet, Shader* packet2, Shader* packet3);
void program_use(Program* program);

// uniforms
void program_set_mat4(Program* program, const char* name, const float* mat);
void program_set_int(Program* program, const char* name, const int num);
void program_set_uint(Program* program, const char* name, const unsigned int num);
void program_set_vec3(Program* program, const char* name, const float* vec3);

#endif