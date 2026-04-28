#include "game.h"
#include "log.h"
#include "win.h"
#include "glad.h"
#include "shader.h"
#include "fm.h"
#include "cam.h"
#include "tex.h"
#include "gfx.h"
#include "chunk.h"
#include "world.h"
#include "input.h"
#include <GLFW/glfw3.h>

Camera main_camera;
HText demo_text;
World world;

Shader a, b;
Program c;

mat4 projection, view;

float cam_speed = 0.015f;
float last_time = 0.0f;

Input input_manager;
Texture texture_atlas;

float sensitivity = 0.00025f;

int game_ticks;

void game_init() {
    world_init(&world);

    int grid_size = 3;
    for (int x = 0; x < grid_size; x++) {
        for (int z = 0; z < grid_size; z++) {
            Chunk chunk;
            chunk_generate(&chunk);
            vec2 pos = { (float)x, (float)z };
            world_add_chunk(&world, &chunk, pos);
        }
    }
    for (int x = 0; x < grid_size; x++) {
        for (int z = 0; z < grid_size; z++) {
            world_rebuild_chunk(&world, x, z);
        }
    }

    texture_atlas.mag_filter = GL_NEAREST;
    texture_atlas.min_filter = GL_NEAREST;
    texture_atlas.wrap_s = GL_REPEAT;
    texture_atlas.wrap_t = GL_REPEAT;

    File vr = file_open("assets/quad/quad.vsh");
    File fr = file_open("assets/quad/quad.fsh");

    texture_create(&texture_atlas, "assets/terrain.png");

    main_camera.pos[2] = -5.0f;

    a.type = GL_VERTEX_SHADER;
    b.type = GL_FRAGMENT_SHADER;

    shader_create(&a, vr.data);
    shader_create(&b, fr.data);

    program_create(&c, &a, &b);

    input_init(&input_manager, _win->glwin);

    text_init("assets/text/text.vsh", "assets/text/text.fsh", "assets/text.png");
    text_create(&demo_text, "BLOCKS", 0x5F, 0, 0);
}

void game_tick() {
    input_update(&input_manager);

    float current_time = (float)glfwGetTime();
    float dt = current_time - last_time;
    last_time = current_time;

    glm_perspective(glm_rad(60.0f), 800.0f / 600.0f, 0.1f, 1000.0f, projection);

    float sensitivity = 0.00025f;

    main_camera.rot[1] -= input_manager.mouse_dx * sensitivity;
    main_camera.rot[0] -= input_manager.mouse_dy * sensitivity;

    if (main_camera.rot[0] > 1.5f) main_camera.rot[0] = 1.5f;
    if (main_camera.rot[0] < -1.5f) main_camera.rot[0] = -1.5f;

    float speed = cam_speed;

    if (input_down(&input_manager, GLFW_KEY_LEFT_SHIFT))
        speed *= 2.5f;

    float velocity = speed * dt;

    if (input_down(&input_manager, GLFW_KEY_W))
        camera_move(&main_camera, main_camera.forward, velocity);

    if (input_down(&input_manager, GLFW_KEY_S))
        camera_move(&main_camera, main_camera.forward, -velocity);

    if (input_down(&input_manager, GLFW_KEY_D))
        camera_move(&main_camera, main_camera.right, velocity);

    if (input_down(&input_manager, GLFW_KEY_A))
        camera_move(&main_camera, main_camera.right, -velocity);

    if (input_down(&input_manager, GLFW_KEY_E))
        camera_move(&main_camera, (vec3){0.0f, 1.0f, 0.0f}, velocity);

    if (input_down(&input_manager, GLFW_KEY_Q))
        camera_move(&main_camera, (vec3){0.0f, 1.0f, 0.0f}, -velocity);

    camera_tick(&main_camera, dt);

    camera_gen(&main_camera, view);
}

void game_draw() {
    program_use(&c);

    texture_bind(&texture_atlas, 0);
    program_set_int(&c, "tex", 0);
    program_set_mat4(&c, "proj", (float*)projection);
    program_set_mat4(&c, "view", (float*)view);

    world_render(&world, &c);

    text_draw(&demo_text);

    glfwSwapBuffers(_win->glwin);
}

void game_destroy() {
    world_destroy(&world);
}
