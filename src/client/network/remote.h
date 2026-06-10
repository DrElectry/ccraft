#ifndef REMOTE_H
#define REMOTE_H

#include <cglm/cglm.h>

#include "core/skin.h"
#include "utils/gltf.h"
#include "gl/shader.h"
#include "gl/tex.h"

typedef struct {
    AnimState* walk_state;
    Skinned* skinned;
    float body_yaw;
    int body_yaw_init;
} RemoteAnim;

typedef struct {
    Skinned_render_request* player_walk_model;
    AnimState* walk_anim;
    Program* skinned_prog;
    Texture* player_tex;
    Texture* player_shininess;
    int bone_head;
    int bone_neck;
} RemoteRenderCtx;

void remote_init(void);

void remote_set_render_ctx(
    Skinned_render_request* player_walk_model,
    AnimState* walk_anim,
    Program* skinned_prog,
    Texture* player_tex,
    Texture* player_shininess,
    int bone_head,
    int bone_neck
);

void remote_draw(const mat4 proj, const mat4 view, float dt, int shadow_pass);

void remote_destroy(void);

#endif