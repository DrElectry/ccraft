#include "log.h"
#include "win.h"
#include "glad.h"
#include "shader.h"
#include "vao.h"
#include "vbo.h"
#include "ebo.h"
#include "fm.h"
#include <GLFW/glfw3.h>
#include <cglm/cglm.h>

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
        -1.0f, 1.0f, 0.0f,
        1.0f, 1.0f, 0.0f,
        1.0f, -1.0f, 0.0f
    };

    int indices[] = {
        0,1,2,
        0,2,3
    };

    VBO vbo;
    VAO vao;

    EBO ebo;

    Shader vertex, fragment;
    Program program;

    File vr = file_open("assets/quad/quad.vsh");
    File fr = file_open("assets/quad/quad.fsh");

    vertex.type = GL_VERTEX_SHADER;
    fragment.type = GL_FRAGMENT_SHADER;

    vao_create(&vao);
    vao_bind(&vao);

    vbo_create(&vbo, vertices, sizeof(vertices));

    ebo_create(&ebo, indices, sizeof(indices));

    vbo_attr(0, 3, 3 * sizeof(float), 0);

    shader_create(&vertex, vr.data);
    shader_create(&fragment, fr.data);

    program_create(&program, &vertex, &fragment);

    while (!window_shouldclose()) {
        window_update();
        window_draw();

        program_use(&program);

        render(&vao);

        glfwSwapBuffers(_win->glwin);
    }
}
