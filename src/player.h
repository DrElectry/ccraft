#ifndef PLAYER_H
#define PLAYER_H

#include "aabb.h"
#include "cam.h"
#include "input.h"
#include "world.h"
#include <cglm/cglm.h>

#define PLAYER_EYE_HEIGHT 1.6f
#define EPSILON 0.0001f
#define GRAVITY 25.0f
#define JUMP_SPEED 8.0f

typedef struct {
    AABB aabb;
    Camera camera;
    float speed;
    float sensitivity;
} Player;

void player_init(Player* p);
void player_tick(World* world, Player* p, Input* in, float dt);
void player_get_view(Player* p, mat4 view);
void player_get_pos(Player* p, vec3 out);
void player_get_eye(Player* p, vec3 out);

#endif

