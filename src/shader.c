#include "shader.h"
#include "glad.h"

#include "log.h"

#include <stdlib.h>

void shader_create(Shader* packet, char* src) {
    packet->id = glCreateShader(packet->type);

    glShaderSource(packet->id, 1, (const char**)&src, NULL);
    glCompileShader(packet->id);

    int  success;
    char infoLog[512];

    glGetShaderiv(packet->id, GL_COMPILE_STATUS, &success);

    if (!success) {
        glGetShaderInfoLog(packet->id, 512, NULL, infoLog);
    }

    ASSERT(success, infoLog);
}

void program_create(Program* packet, Shader* packet2, Shader* packet3) {
    packet->id = glCreateProgram();
    glAttachShader(packet->id, packet2->id);
    glAttachShader(packet->id, packet3->id);
    glLinkProgram(packet->id);

    int  success;
    char infoLog[512];

    glGetProgramiv(packet->id, GL_LINK_STATUS, &success);

    if (!success) {
        glGetProgramInfoLog(packet->id, 512, NULL, infoLog);
    }

    ASSERT(success, infoLog);
}

void program_use(Program* program) {
    glUseProgram(program->id);
}

void program_set_mat4(Program* program, const char* name, const float* mat) {
    int loc = glGetUniformLocation(program->id, name);
    ASSERT(loc != -1, "no such uniform, %s", name);
    glUniformMatrix4fv(loc, 1, GL_FALSE, mat);
}

void program_set_mat3(Program* program, const char* name, const float* mat) {
    int loc = glGetUniformLocation(program->id, name);
    ASSERT(loc != -1, "no such uniform, %s", name);
    glUniformMatrix3fv(loc, 1, GL_FALSE, mat);
}


void program_set_int(Program* program, const char* name, const int num) {
    int loc = glGetUniformLocation(program->id, name);
    ASSERT(loc != -1, "no such uniform, %s", name);
    glUniform1i(loc, num);
}

void program_set_uint(Program* program, const char* name, const unsigned int num) {
    int loc = glGetUniformLocation(program->id, name);
    ASSERT(loc != -1, "no such uniform, %s", name);
    glUniform1ui(loc, num); 
}

void program_set_vec3(Program* program, const char* name, const float* vec3) {
    int loc = glGetUniformLocation(program->id, name);
    ASSERT(loc != -1, "no such uniform, %s", name);
    glUniform3f(loc, vec3[0], vec3[1], vec3[2]);
}

void program_set_vec2(Program* program, const char* name, const float* vec2) {
    int loc = glGetUniformLocation(program->id, name);
    ASSERT(loc != -1, "no such uniform, %s", name);
    glUniform2f(loc, vec2[0], vec2[1]);
}
