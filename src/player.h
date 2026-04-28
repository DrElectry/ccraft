#ifndef PLAYER_H
#define PLAYER_H

#include "aabb.h"
#include "cam.h"
#include "input.h"
#include <cglm/cglm.h>

typedef struct {
    AABB aabb;
    Camera camera;
    float speed;
    float sensitivity;
} Player;

void player_init(Player* p);
void player_tick(Player* p, Input* in, float dt);
void player_get_view(Player* p, mat4 view);
void player_get_pos(Player* p, vec3 out);

#endif

