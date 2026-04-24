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