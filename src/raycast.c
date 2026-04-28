#include "raycast.h"
#include "tile.h"
#include <math.h>

static int is_solid(uint16_t b) {
    return b != 0;
}

void raycast_dda(World* world, vec3 origin, vec3 dir, float max_dist, RaycastHit* out) {
    out->hit = 0;


    vec3 d;
    glm_vec3_copy(dir, d);
    float len = glm_vec3_norm(d);
    if (len < 0.0001f) return;
    glm_vec3_scale(d, 1.0f / len, d);

    int x = (int)floorf(origin[0]);
    int y = (int)floorf(origin[1]);
    int z = (int)floorf(origin[2]);

    int stepX = (d[0] >= 0.0f) ? 1 : -1;
    int stepY = (d[1] >= 0.0f) ? 1 : -1;
    int stepZ = (d[2] >= 0.0f) ? 1 : -1;

    float nextX = (stepX > 0) ? ((x + 1) - origin[0]) / d[0] : (x - origin[0]) / d[0];
    float nextY = (stepY > 0) ? ((y + 1) - origin[1]) / d[1] : (y - origin[1]) / d[1];
    float nextZ = (stepZ > 0) ? ((z + 1) - origin[2]) / d[2] : (z - origin[2]) / d[2];

    float deltaX = fabsf(1.0f / d[0]);
    float deltaY = fabsf(1.0f / d[1]);
    float deltaZ = fabsf(1.0f / d[2]);

    float dist = 0.0f;

    while (dist < max_dist) {
        if (dist > 0.001f) {
            uint16_t block = world_get_block(world, x, y, z);
            if (is_solid(block)) {
                out->hit = 1;
                out->bx = x;
                out->by = y;
                out->bz = z;
                out->dist = dist;

                // Compute hit point
                out->pos[0] = origin[0] + d[0] * dist;
                out->pos[1] = origin[1] + d[1] * dist;
                out->pos[2] = origin[2] + d[2] * dist;

                return;
            }
        }

        if (nextX < nextY && nextX < nextZ) {
            dist = nextX;
            x += stepX;
            nextX += deltaX;

            out->normal[0] = -stepX;
            out->normal[1] = 0;
            out->normal[2] = 0;
            out->face = (stepX > 0) ? RIGHT : LEFT;
        } else if (nextY < nextZ) {
            dist = nextY;
            y += stepY;
            nextY += deltaY;

            out->normal[0] = 0;
            out->normal[1] = -stepY;
            out->normal[2] = 0;
            out->face = (stepY > 0) ? UP : DOWN;
        } else {
            dist = nextZ;
            z += stepZ;
            nextZ += deltaZ;

            out->normal[0] = 0;
            out->normal[1] = 0;
            out->normal[2] = -stepZ;
            out->face = (stepZ > 0) ? FRONT : BACK;
        }
    }
}
