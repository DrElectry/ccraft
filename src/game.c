#include "game.h"
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

Render_request tile;
Camera main_camera;
HText demo_text;

Shader a, b;
Program c;

float v[1000];
int i[1000];

int vc, ic;

mat4 projection, view;

float cam_speed = 6.0f;
float last_time = 0.0f;

Input input_manager;
Texture texture_atlas;

float sensitivity = 0.00025f;

void game_init() {
    tile_push_face(v, i, (vec3){0.0f, 0.0f, 0.0f}, &vc, &ic, FRONT, 1);
    tile_push_face(v, i, (vec3){0.0f, 0.0f, 0.0f}, &vc, &ic, BACK, 1);
    tile_push_face(v, i, (vec3){0.0f, 0.0f, 0.0f}, &vc, &ic, LEFT, 1);
    tile_push_face(v, i, (vec3){0.0f, 0.0f, 0.0f}, &vc, &ic, RIGHT, 1);
    tile_push_face(v, i, (vec3){0.0f, 0.0f, 0.0f}, &vc, &ic, UP, 0);
    tile_push_face(v, i, (vec3){0.0f, 0.0f, 0.0f}, &vc, &ic, DOWN, 2);

    tile.data = v;
    tile.triangles = i;
    tile.data_size = vc * sizeof(float);
    tile.tri_count = ic / 3;
    glm_vec3_copy((vec3){0.0f, 0.0f, 0.0f}, tile.pos);
    tile.rot = 0.0f;
    glm_vec3_copy((vec3){1.0f, 1.0f, 1.0f}, tile.scale);
    gfx_packet_static_request(&tile);

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
    text_create(&demo_text, "BLOCKS", 0x4F, 0, 0);
}

void game_tick() {
    input_update(&input_manager);

    float current_time = (float)glfwGetTime();
    float dt = current_time - last_time;
    last_time = current_time;

    glm_perspective(glm_rad(60.0f), 800.0f / 600.0f, 0.1f, 1000.0f, projection);

    camera_calculate(&main_camera);

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
        glm_vec3_muladds(main_camera.forward, velocity, main_camera.pos);

    if (input_down(&input_manager, GLFW_KEY_S))
        glm_vec3_muladds(main_camera.forward, -velocity, main_camera.pos);

    if (input_down(&input_manager, GLFW_KEY_D))
        glm_vec3_muladds(main_camera.right, velocity, main_camera.pos);

    if (input_down(&input_manager, GLFW_KEY_A))
        glm_vec3_muladds(main_camera.right, -velocity, main_camera.pos);

    if (input_down(&input_manager, GLFW_KEY_E))
        main_camera.pos[1] += velocity;

    if (input_down(&input_manager, GLFW_KEY_Q))
        main_camera.pos[1] -= velocity;

    camera_gen(&main_camera, view);
}

void game_draw() {
    program_use(&c);

    texture_bind(&texture_atlas, 0);
    program_set_int(&c, "tex", 0);
    program_set_mat4(&c, "proj", (float*)projection);
    program_set_mat4(&c, "view", (float*)view);

    gfx_render(&tile, &c);

    text_draw(&demo_text);

    glfwSwapBuffers(_win->glwin);
}
