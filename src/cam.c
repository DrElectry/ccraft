#include "cam.h"
#include <cglm/cglm.h>

void camera_calculate(Camera* cam) {
    vec3 world_up = {0.0f, 1.0f, 0.0f};

    float pitch = cam->rot[0];
    float yaw   = cam->rot[1];

    // Forward
    cam->forward[0] = cosf(pitch) * sinf(yaw);
    cam->forward[1] = sinf(pitch);
    cam->forward[2] = cosf(pitch) * cosf(yaw);
    glm_vec3_normalize(cam->forward);

    // Right
    glm_vec3_cross(cam->forward, world_up, cam->right);
    glm_vec3_normalize(cam->right);

    // Up
    glm_vec3_cross(cam->right, cam->forward, cam->up);
    glm_vec3_normalize(cam->up);
}

void camera_gen(Camera* cam, mat4 matrix) {
    vec3 a;
    glm_vec3_add(cam->pos, cam->forward, a);
    glm_lookat(cam->pos, a, cam->up, matrix);
};