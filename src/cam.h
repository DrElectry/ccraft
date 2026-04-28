#ifndef CAM_H
#define CAM_H

#include <cglm/cglm.h>

typedef struct {
    vec3 pos, rot, forward, right, up, vel;
} Camera;

void camera_calculate(Camera* cam);
void camera_gen(Camera* cam, mat4 matrix);
void camera_tick(Camera* cam, float dt);
void camera_move(Camera* cam, vec3 dir, float accel);
void camera_gen(Camera* cam, mat4 matrix);

#endif