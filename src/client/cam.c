#include "cam.h"
#include <cglm/cglm.h>

void camera_calculate(Camera* cam) {
    vec3 world_up = {0.0f, 1.0f, 0.0f};

    float pitch = cam->rot[0];
    float yaw   = cam->rot[1];

    cam->forward[0] = cosf(pitch) * sinf(yaw);
    cam->forward[1] = sinf(pitch);
    cam->forward[2] = cosf(pitch) * cosf(yaw);
    glm_vec3_normalize(cam->forward);

    glm_vec3_cross(cam->forward, world_up, cam->right);
    glm_vec3_normalize(cam->right);

    glm_vec3_cross(cam->right, cam->forward, cam->up);
    glm_vec3_normalize(cam->up);
}

void camera_tick(Camera* cam, float dt) {
    vec3 displacement;
    glm_vec3_scale(cam->vel, dt, displacement);
    glm_vec3_add(cam->pos, displacement, cam->pos);
    float damping = expf(-8.0f * dt);
    glm_vec3_scale(cam->vel, damping, cam->vel);
    glm_vec3_scale(cam->vel, damping, cam->vel);
    camera_calculate(cam);
}

void camera_move(Camera* cam, vec3 dir, float accel) {
    vec3 v;
    glm_vec3_scale(dir, accel, v);
    glm_vec3_add(cam->vel, v, cam->vel);
}

void camera_gen(Camera* cam, mat4 matrix) {
    vec3 target;
    glm_vec3_add(cam->pos, cam->forward, target);
    glm_lookat(cam->pos, target, cam->up, matrix);
}