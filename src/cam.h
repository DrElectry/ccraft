#ifndef CAM_H
#define CAM_H

#include <cglm/cglm.h>

typedef struct {
    vec3 pos, rot, forward, right, up;
} Camera;

void camera_calculate(Camera* cam);
void camera_gen(Camera* cam, mat4 matrix);

#endif