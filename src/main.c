#include "log.h"
#include "win.h"
#include "glad.h"
#include "shader.h"
#include "vao.h"
#include "vbo.h"
#include <GLFW/glfw3.h>

const char* vertex_shader = 
"#version 330 core\n"
"\n"
"layout(location=0) in vec3 in_vert;\n"
"\n"
"void main() {\n"
"gl_Position=vec4(in_vert, 1.0);\n"
"}\n";

const char* fragment_shader = 
"#version 330 core\n"
"\n"
"out vec4 frag;\n"
"\n"
"void main() {\n"
"    frag = vec4(0.0, 1.0, 0.0, 1.0);\n"
"}\n";

int main() {
    ASSERT(glfwInit(), "no glfw");

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    Window packet;

    packet.width = 800;
    packet.height = 600;
    packet.title = "blocks";

    window_create(&packet);

    ASSERT(gladLoadGLLoader((GLADloadproc)glfwGetProcAddress), "no glad");

    glClearColor(0.0f, 1.0f, 1.0f, 1.0f);

    float vertices[] = {
        -1.0f, -1.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        1.0f, -1.0f, 0.0f
    };

    VBO vbo;
    VAO vao;

    Shader vertex, fragment;
    Program program;

    vertex.type = GL_VERTEX_SHADER;
    fragment.type = GL_FRAGMENT_SHADER;

    vao_create(&vao);
    vao_bind(&vao);

    vbo_create(&vbo, vertices, sizeof(vertices));
    vbo_attr(0, 3, 3 * sizeof(float), 0);

    shader_create(&vertex, vertex_shader);
    shader_create(&fragment, fragment_shader);

    program_create(&program, &vertex, &fragment);

    while (!window_shouldclose()) {
        window_update();
        window_draw();

        program_use(&program);

        render(&vao);

        glfwSwapBuffers(_win->glwin);
    }
}
