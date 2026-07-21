#ifndef SOFTBODY_H
#define SOFTBODY_H

#include "core/gfx.h"
#include "core/skin.h"
#include "core/world.h"

#include <cglm/cglm.h>

typedef struct {
    int   bone_count;
    float spring_k;
    float damping;
    float gravity;
    float bounce_factor;
} SoftbodyConfig;

typedef struct Softbody Softbody;

Softbody* softbody_load(const char* path, const SoftbodyConfig* cfg);
void softbody_destroy(Softbody* sb);
void softbody_set_transform(Softbody* sb, vec3 pos, vec3 rot, vec3 scale);
void softbody_update(Softbody* sb, World* world, float dt);
Skinned_render_request* softbody_render_request(Softbody* sb);
void softbody_render(Softbody* sb, Program* prog);

#endif