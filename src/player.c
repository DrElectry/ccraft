#include "player.h"
#include "world.h"
#include <cglm/cglm.h>
#include <math.h>

#define PLAYER_EYE_HEIGHT 1.6f

int is_solid(uint16_t b) {
    return b != 0;
}

int aabb_collides(World* world, Player* p) {
    int minX = (int)floorf(p->aabb.x);
    int maxX = (int)floorf(p->aabb.x + p->aabb.w);

    int minY = (int)floorf(p->aabb.y);
    int maxY = (int)floorf(p->aabb.y + p->aabb.h);

    int minZ = (int)floorf(p->aabb.z);
    int maxZ = (int)floorf(p->aabb.z + p->aabb.d);

    for (int x = minX; x <= maxX; x++) {
        for (int y = minY; y <= maxY; y++) {
            for (int z = minZ; z <= maxZ; z++) {
                vec3 pnt = {x, y, z};
                if (is_solid(world_get_block_at(world, pnt))) {
                    return 1;
                }
            }
        }
    }

    return 0;
}

void player_move(World* world, Player* p, vec3 move) {
    vec3 original;
    glm_vec3_copy(p->camera.pos, original);

    p->camera.pos[0] += move[0];
    p->aabb.x = p->camera.pos[0] - p->aabb.w * 0.5f;
    if (aabb_collides(world, p)) {
        p->camera.pos[0] = original[0];
        p->aabb.x = p->camera.pos[0] - p->aabb.w * 0.5f;
    }

    p->camera.pos[1] += move[1];
    p->aabb.y = p->camera.pos[1];
    if (aabb_collides(world, p)) {
        p->camera.pos[1] = original[1];
        p->aabb.y = p->camera.pos[1];
    }

    p->camera.pos[2] += move[2];
    p->aabb.z = p->camera.pos[2] - p->aabb.d * 0.5f;
    if (aabb_collides(world, p)) {
        p->camera.pos[2] = original[2];
        p->aabb.z = p->camera.pos[2] - p->aabb.d * 0.5f;
    }
}

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

    p->speed = 5.0f;
    p->sensitivity = 0.00025f;

    camera_calculate(&p->camera);
}

void player_tick(World* world, Player* p, Input* in, float dt) {
    p->camera.rot[1] -= in->mouse_dx * p->sensitivity;
    p->camera.rot[0] -= in->mouse_dy * p->sensitivity;

    if (p->camera.rot[0] > 1.5f) p->camera.rot[0] = 1.5f;
    if (p->camera.rot[0] < -1.5f) p->camera.rot[0] = -1.5f;

    float speed = p->speed;

    if (input_down(in, GLFW_KEY_LEFT_SHIFT))
        speed *= 2.5f;

    float velocity = speed * dt;

    vec3 move = {0.0f, 0.0f, 0.0f};

    if (input_down(in, GLFW_KEY_W))
        glm_vec3_add(move, (vec3){p->camera.forward[0] * velocity, p->camera.forward[1] * velocity, p->camera.forward[2] * velocity}, move);

    if (input_down(in, GLFW_KEY_S))
        glm_vec3_add(move, (vec3){-p->camera.forward[0] * velocity, -p->camera.forward[1] * velocity, -p->camera.forward[2] * velocity}, move);

    if (input_down(in, GLFW_KEY_D))
        glm_vec3_add(move, (vec3){p->camera.right[0] * velocity, p->camera.right[1] * velocity, p->camera.right[2] * velocity}, move);

    if (input_down(in, GLFW_KEY_A))
        glm_vec3_add(move, (vec3){-p->camera.right[0] * velocity, -p->camera.right[1] * velocity, -p->camera.right[2] * velocity}, move);

    if (input_down(in, GLFW_KEY_E))
        move[1] += velocity;

    if (input_down(in, GLFW_KEY_Q))
        move[1] -= velocity;

    player_move(world, p, move);

    camera_tick(&p->camera, dt);

    p->aabb.x = p->camera.pos[0] - p->aabb.w * 0.5f;
    p->aabb.y = p->camera.pos[1];
    p->aabb.z = p->camera.pos[2] - p->aabb.d * 0.5f;
}

void player_get_view(Player* p, mat4 view) {
    vec3 eye;
    glm_vec3_copy(p->camera.pos, eye);
    eye[1] += PLAYER_EYE_HEIGHT;

    vec3 target;
    glm_vec3_add(eye, p->camera.forward, target);
    glm_lookat(eye, target, p->camera.up, view);
}

void player_get_pos(Player* p, vec3 out) {
    glm_vec3_copy(p->camera.pos, out);
}