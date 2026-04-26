#include "log.h"
#include "win.h"
#include "glad.h"
#include "shader.h"
#include "fm.h"
#include "cam.h"
#include "tex.h"
#include "tile.h"
#include "gfx.h"
#include "input.h"
#include <GLFW/glfw3.h>
#include <cglm/cglm.h>

Camera main_camera;

int main() {
    ASSERT(glfwInit(), "no glfw");

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GL_DEPTH_BITS, 24);

    Window packet;

    packet.width = 800;
    packet.height = 600;
    packet.title = "blocks";

    window_create(&packet);

    ASSERT(gladLoadGLLoader((GLADloadproc)glfwGetProcAddress), "no glad");

    glfwSetInputMode(packet.glwin, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
    glfwSetInputMode(packet.glwin, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glClearColor(0.1f, 0.2f, 0.2f, 1.0f);

    float vertices[1000];
    int indices[1000];
    int v = 0, i = 0;

    tile_push_face(vertices, indices, (vec3){0.0f, 0.0f, 0.0f}, &v, &i, FRONT, 2);
    tile_push_face(vertices, indices, (vec3){0.0f, 0.0f, 0.0f}, &v, &i, BACK, 2);
    tile_push_face(vertices, indices, (vec3){0.0f, 0.0f, 0.0f}, &v, &i, LEFT, 2);
    tile_push_face(vertices, indices, (vec3){0.0f, 0.0f, 0.0f}, &v, &i, RIGHT, 2);
    tile_push_face(vertices, indices, (vec3){0.0f, 0.0f, 0.0f}, &v, &i, UP, 1);
    tile_push_face(vertices, indices, (vec3){0.0f, 0.0f, 0.0f}, &v, &i, DOWN, 0);

    Render_request block;
    block.data = vertices;
    block.triangles = indices;
    block.data_size = v * sizeof(float);
    block.tri_count = i / 3;

    glm_vec3_zero(block.pos);
    block.rot = 0.0f;
    glm_vec3_copy((vec3){1.0f, 1.0f, 1.0f}, block.scale);

    gfx_packet_static_request(&block);

    Shader vertex, fragment;
    Program program;
    Camera cam;
    Input in;
    Texture tex;

    input_init(&in, packet.glwin);

    tex.mag_filter = GL_NEAREST;
    tex.min_filter = GL_NEAREST;
    tex.wrap_s = GL_REPEAT;
    tex.wrap_t = GL_REPEAT;

    File vr = file_open("assets/quad/quad.vsh");
    File fr = file_open("assets/quad/quad.fsh");

    texture_create(&tex, "assets/terrain.png");

    vertex.type = GL_VERTEX_SHADER;
    fragment.type = GL_FRAGMENT_SHADER;

    shader_create(&vertex, vr.data);
    shader_create(&fragment, fr.data);

    program_create(&program, &vertex, &fragment);

    glm_vec3_zero(cam.pos);
    cam.pos[2] = -5.0f;
    glm_vec3_zero(cam.rot);

    mat4 projection;
    mat4 view;

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    float cam_speed = 6.0f;
    float last_time = (float)glfwGetTime();

    while (!window_shouldclose()) {
        window_update();
        window_draw();

        input_update(&in);

        float current_time = (float)glfwGetTime();
        float dt = current_time - last_time;
        last_time = current_time;

        glm_perspective(glm_rad(60.0f), 800.0f / 600.0f, 0.1f, 1000.0f, projection);

        camera_calculate(&cam);

        float sensitivity = 0.00025f;

        cam.rot[1] -= in.mouse_dx * sensitivity;
        cam.rot[0] -= in.mouse_dy * sensitivity;

        if (cam.rot[0] > 1.5f) cam.rot[0] = 1.5f;
        if (cam.rot[0] < -1.5f) cam.rot[0] = -1.5f;

        float speed = cam_speed;

        if (input_down(&in, GLFW_KEY_LEFT_SHIFT))
            speed *= 2.5f;

        float velocity = speed * dt;

        if (input_down(&in, GLFW_KEY_W))
            glm_vec3_muladds(cam.forward, velocity, cam.pos);

        if (input_down(&in, GLFW_KEY_S))
            glm_vec3_muladds(cam.forward, -velocity, cam.pos);

        if (input_down(&in, GLFW_KEY_D))
            glm_vec3_muladds(cam.right, velocity, cam.pos);

        if (input_down(&in, GLFW_KEY_A))
            glm_vec3_muladds(cam.right, -velocity, cam.pos);

        if (input_down(&in, GLFW_KEY_E))
            cam.pos[1] += velocity;

        if (input_down(&in, GLFW_KEY_Q))
            cam.pos[1] -= velocity;

        camera_gen(&cam, view);

        program_use(&program);

        texture_bind(&tex, 0);
        program_set_int(&program, "tex", 0);
        program_set_mat4(&program, "proj", (float*)projection);
        program_set_mat4(&program, "view", (float*)view);

        gfx_render(&block, &program);

        glfwSwapBuffers(packet.glwin);
    }
}