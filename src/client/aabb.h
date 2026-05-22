#ifndef AABB_H
#define AABB_H

typedef struct {
    float x, y, z, w, h, d;
} AABB;

int aabb_intersects(AABB a, AABB b);

#endif