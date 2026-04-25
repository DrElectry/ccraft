#include "log.h"
#include "win.h"
#include "glad.h"
#include "shader.h"
#include "vao.h"
#include "vbo.h"
#include "ebo.h"
#include "fm.h"
#include "cam.h"
#include "tex.h"
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

    glClearColor(0.702f, 0.898f, 0.988f, 1.0f);

    float vertices[] = {
        -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
        -1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
        1.0f, 1.0f, 0.0f, 1.0f, 1.0f,
        1.0f, -1.0f, 0.0f, 1.0f, 0.0f
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
    Camera cam;

    Texture tex;

    tex.mag_filter = GL_NEAREST;
    tex.min_filter = GL_NEAREST;
    tex.wrap_s = GL_REPEAT;
    tex.wrap_t = GL_REPEAT;

    File vr = file_open("assets/quad/quad.vsh");
    File fr = file_open("assets/quad/quad.fsh");

    texture_create(&tex, "assets/terrain.png");

    vertex.type = GL_VERTEX_SHADER;
    fragment.type = GL_FRAGMENT_SHADER;

    vao_create(&vao);
    vao_bind(&vao);

    vbo_create(&vbo, vertices, sizeof(vertices));

    ebo_create(&ebo, indices, sizeof(indices));

    vbo_attr(0, 3, 5 * sizeof(float), 0);
    vbo_attr(1, 2, 5 * sizeof(float), 3);

    shader_create(&vertex, vr.data);
    shader_create(&fragment, fr.data);

    program_create(&program, &vertex, &fragment);

    glm_vec3_zero(cam.pos);
    cam.pos[2] = -5.0f;
    glm_vec3_zero(cam.rot);

    mat4 projection;
    mat4 view;
    mat4 model;
    glm_mat4_identity(model);
    
    while (!window_shouldclose()) {
        window_update();
        window_draw();

        glm_perspective(glm_rad(60.0f), 800.0f/600.0f, 0.1, 1000.0, projection);

        camera_calculate(&cam);
        camera_gen(&cam, view);

        program_use(&program);

        texture_bind(&tex, 0);

        program_set_int(&program, "tex", 0);

        program_set_mat4(&program, "proj", (float*)projection);
        program_set_mat4(&program, "view", (float*)view);
        program_set_mat4(&program, "model", (float*)model);

        render(&vao);

        glfwSwapBuffers(_win->glwin);
    }
}
