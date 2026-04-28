#include "player.h"
#include "world.h"
#include <cglm/cglm.h>
#include <math.h>

#define PLAYER_EYE_HEIGHT 1.6f
#define EPSILON 0.0001f

static int is_solid(uint16_t b) {
    return b != 0;
}

static void sync_aabb(Player* p) {
    p->aabb.x = p->camera.pos[0] - p->aabb.w * 0.5f;
    p->aabb.y = p->camera.pos[1];
    p->aabb.z = p->camera.pos[2] - p->aabb.d * 0.5f;
}

static int aabb_collides(World* world, Player* p) {
    int minX = (int)floorf(p->aabb.x + EPSILON);
    int maxX = (int)floorf(p->aabb.x + p->aabb.w - EPSILON);

    int minY = (int)floorf(p->aabb.y + EPSILON);
    int maxY = (int)floorf(p->aabb.y + p->aabb.h - EPSILON);

    int minZ = (int)floorf(p->aabb.z + EPSILON);
    int maxZ = (int)floorf(p->aabb.z + p->aabb.d - EPSILON);

    for (int x = minX; x <= maxX; x++) {
        for (int y = minY; y <= maxY; y++) {
            for (int z = minZ; z <= maxZ; z++) {
                vec3 pnt = { (float)x, (float)y, (float)z };
                if (is_solid(world_get_block_at(world, pnt))) {
                    return 1;
                }
            }
        }
    }

    return 0;
}

static void move_axis(World* world, Player* p, int axis, float delta) {
    if (delta == 0.0f) return;

    p->camera.pos[axis] += delta;
    sync_aabb(p);

    if (aabb_collides(world, p)) {
        p->camera.pos[axis] -= delta;
        p->camera.vel[axis] = 0.0f;
        sync_aabb(p);

        if (delta > 0.0f) {
            p->camera.pos[axis] -= EPSILON;
        } else if (delta < 0.0f) {
            p->camera.pos[axis] += EPSILON;
        }
        sync_aabb(p);
    }
}

void player_move_vel(World* world, Player* p, float dt) {
    vec3 delta;
    glm_vec3_scale(p->camera.vel, dt, delta);

    move_axis(world, p, 1, delta[1]);
    move_axis(world, p, 0, delta[0]);
    move_axis(world, p, 2, delta[2]);

    if (aabb_collides(world, p)) {
        for (int axis = 0; axis < 3; axis++) {
            float original = p->camera.pos[axis];
            p->camera.pos[axis] = original + (delta[axis] > 0 ? -EPSILON : EPSILON);
            sync_aabb(p);
            if (!aabb_collides(world, p)) break;
            p->camera.pos[axis] = original;
        }
    }
}

void player_init(Player* p) {
    p->camera.pos[0] = 50.0f;
    p->camera.pos[1] = 10.0f;
    p->camera.pos[2] = 50.0f;

    p->camera.rot[0] = 0.0f;
    p->camera.rot[1] = 0.0f;
    p->camera.rot[2] = 0.0f;

    glm_vec3_zero(p->camera.vel);

    p->aabb.w = 0.6f;
    p->aabb.h = 1.8f;
    p->aabb.d = 0.6f;

    p->speed = 100.0f;
    p->sensitivity = 0.00025f;

    sync_aabb(p);
    camera_calculate(&p->camera);
}

void player_tick(World* world, Player* p, Input* in, float dt) {
    p->camera.rot[1] -= in->mouse_dx * p->sensitivity;
    p->camera.rot[0] -= in->mouse_dy * p->sensitivity;

    if (p->camera.rot[0] > 1.5f) p->camera.rot[0] = 1.5f;
    if (p->camera.rot[0] < -1.5f) p->camera.rot[0] = -1.5f;

    float speed = p->speed;
    if (input_down(in, GLFW_KEY_LEFT_SHIFT)) {
        speed *= 2.5f;
    }

    vec3 accel = {0.0f, 0.0f, 0.0f};

    if (input_down(in, GLFW_KEY_W))
        glm_vec3_add(accel, (vec3){p->camera.forward[0], 0.0f, p->camera.forward[2]}, accel);

    if (input_down(in, GLFW_KEY_S))
        glm_vec3_add(accel, (vec3){-p->camera.forward[0], 0.0f, -p->camera.forward[2]}, accel);

    if (input_down(in, GLFW_KEY_D))
        glm_vec3_add(accel, (vec3){p->camera.right[0], 0.0f, p->camera.right[2]}, accel);

    if (input_down(in, GLFW_KEY_A))
        glm_vec3_add(accel, (vec3){-p->camera.right[0], 0.0f, -p->camera.right[2]}, accel);

    vec3 flat = {accel[0], 0.0f, accel[2]};
    if (glm_vec3_norm(flat) > 0.0f) {
        glm_vec3_normalize(flat);
        glm_vec3_scale(flat, speed, flat);
        accel[0] = flat[0];
        accel[2] = flat[2];
    }

    p->camera.vel[0] += accel[0] * dt;
    p->camera.vel[2] += accel[2] * dt;

    if (input_down(in, GLFW_KEY_E))
        p->camera.vel[1] += speed * dt;

    if (input_down(in, GLFW_KEY_Q))
        p->camera.vel[1] -= speed * dt;

    float damping = 5.0f;
    float factor = 1.0f - damping * dt;
    if (factor < 0.0f) factor = 0.0f;
    glm_vec3_scale(p->camera.vel, factor, p->camera.vel);

    player_move_vel(world, p, dt);

    camera_calculate(&p->camera);
    sync_aabb(p);
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