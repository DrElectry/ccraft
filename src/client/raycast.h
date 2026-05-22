#ifndef RAYCAST_H
#define RAYCAST_H

#include "world.h"
#include <cglm/cglm.h>

typedef struct {
    int hit;
    int bx, by, bz;
    vec3 pos;
    vec3 normal;
    int face;
    float dist;
} RaycastHit;

void raycast_dda(World* world, vec3 origin, vec3 dir, float max_dist, RaycastHit* out);

#endif
