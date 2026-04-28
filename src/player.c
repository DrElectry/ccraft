#include "player.h"
#include <cglm/cglm.h>

void player_init(Player* p) {
    p->camera.pos[0] = 0.0f;
    p->camera.pos[1] = 0.0f;
    p->camera.pos[2] = -5.0f;

    p->camera.rot[0] = 0.0f;
    p->camera.rot[1] = 0.0f;
    p->camera.rot[2] = 0.0f;

    p->aabb.x = 0.0f;
    p->aabb.y = 0.0f;
    p->aabb.z = 0.0f;
    p->aabb.w = 0.6f;
    p->aabb.h = 1.8f;
    p->aabb.d = 0.6f;

    p->speed = 150.0f;
    p->sensitivity = 0.00025f;

    camera_calculate(&p->camera);
}

void player_tick(Player* p, Input* in, float dt) {
    p->camera.rot[1] -= in->mouse_dx * p->sensitivity;
    p->camera.rot[0] -= in->mouse_dy * p->sensitivity;

    if (p->camera.rot[0] > 1.5f) p->camera.rot[0] = 1.5f;
    if (p->camera.rot[0] < -1.5f) p->camera.rot[0] = -1.5f;

    float speed = p->speed;

    if (input_down(in, GLFW_KEY_LEFT_SHIFT))
        speed *= 2.5f;

    float velocity = speed * dt;

    if (input_down(in, GLFW_KEY_W))
        camera_move(&p->camera, p->camera.forward, velocity);

    if (input_down(in, GLFW_KEY_S))
        camera_move(&p->camera, p->camera.forward, -velocity);

    if (input_down(in, GLFW_KEY_D))
        camera_move(&p->camera, p->camera.right, velocity);

    if (input_down(in, GLFW_KEY_A))
        camera_move(&p->camera, p->camera.right, -velocity);

    if (input_down(in, GLFW_KEY_E))
        camera_move(&p->camera, (vec3){0.0f, 1.0f, 0.0f}, velocity);

    if (input_down(in, GLFW_KEY_Q))
        camera_move(&p->camera, (vec3){0.0f, 1.0f, 0.0f}, -velocity);

    camera_tick(&p->camera, dt);

    // Sync AABB to camera position
    p->aabb.x = p->camera.pos[0] - p->aabb.w * 0.5f;
    p->aabb.y = p->camera.pos[1];
    p->aabb.z = p->camera.pos[2] - p->aabb.d * 0.5f;
}

void player_get_view(Player* p, mat4 view) {
    camera_gen(&p->camera, view);
}

void player_get_pos(Player* p, vec3 out) {
    glm_vec3_copy(p->camera.pos, out);
}

