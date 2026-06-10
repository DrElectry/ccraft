#include "network/remote.h"

#include <stdlib.h>
#include <string.h>
#include <float.h>

#include "network/network.h"

#include "core/game.h"
#include "core/world.h"
#include "core/gfx.h"
#include "core/skin.h"
#include "utils/gltf.h"
#include "gl/shader.h"
#include "gl/tex.h"

static RemoteRenderCtx g_ctx = {0};

void remote_set_render_ctx(
    Skinned_render_request* player_walk_model,
    AnimState* walk_anim,
    Program* skinned_prog,
    Texture* player_tex,
    Texture* player_shininess,
    int bone_head,
    int bone_neck
) {
    g_ctx.player_walk_model = player_walk_model;
    g_ctx.walk_anim = walk_anim;
    g_ctx.skinned_prog = skinned_prog;
    g_ctx.player_tex = player_tex;
    g_ctx.player_shininess = player_shininess;
    g_ctx.bone_head = bone_head;
    g_ctx.bone_neck = bone_neck;
}

static RemoteAnim remote_anim[CLIENT_MAX_REMOTES];

void remote_anim_cleanup(int i) {
    if (remote_anim[i].walk_state) {
        anim_state_destroy(remote_anim[i].walk_state);
        remote_anim[i].walk_state = NULL;
    }
    if (remote_anim[i].skinned) {
        free(remote_anim[i].skinned);
        remote_anim[i].skinned = NULL;
    }
    remote_anim[i].body_yaw = 0.0f;
    remote_anim[i].body_yaw_init = 0;
}

float get_feet_align_y(Skinned_render_request* req) {
    if (!req || !req->skinned || req->skinned->vertex_count <= 0) {
        return 0.0f;
    }

    float min_y = FLT_MAX;
    for (int i = 0; i < req->skinned->vertex_count; i++) {
        vec4 local = {
            req->skinned->vertices[i * 16 + 0],
            req->skinned->vertices[i * 16 + 1],
            req->skinned->vertices[i * 16 + 2],
            1.0f
        };
        vec4 world;
        glm_mat4_mulv(req->skinned->node_transform, local, world);
        if (world[1] < min_y) {
            min_y = world[1];
        }
    }

    if (min_y == FLT_MAX) {
        return 0.0f;
    }

    return -min_y;
}

void draw_remote_player(int i, const RemotePlayer* rp, float dt, const mat4 proj, const mat4 view_mat, int shadow_pass) {
    (void)shadow_pass;

    if (!rp->active || !g_ctx.player_walk_model || !g_ctx.walk_anim) {
        return;
    }

    int walking = rp->on_ground != 0;

    if (!remote_anim[i].skinned) {
        remote_anim[i].skinned = malloc(sizeof(Skinned));
        memcpy(remote_anim[i].skinned, g_ctx.player_walk_model->skinned, sizeof(Skinned));
        remote_anim[i].skinned->gpu.anim = NULL;
        remote_anim[i].skinned->gpu.skeleton = g_ctx.player_walk_model->skeleton;
    }

    Skinned* sk = remote_anim[i].skinned;
    sk->gpu.skeleton = g_ctx.player_walk_model->skeleton;

    if (!remote_anim[i].walk_state) {
        remote_anim[i].walk_state = anim_state_create(g_ctx.walk_anim->clip);
        remote_anim[i].walk_state->loop = 1;
        remote_anim[i].walk_state->speed = 1.0f;
    }

    if (walking) {
        anim_state_play(remote_anim[i].walk_state);
        anim_state_update(remote_anim[i].walk_state, dt);
    } else {
        remote_anim[i].walk_state->current_time = 0.1f;
        remote_anim[i].walk_state->is_playing = 0;
    }
    sk->gpu.anim = remote_anim[i].walk_state;

    if (!remote_anim[i].body_yaw_init) {
        remote_anim[i].body_yaw = rp->rot[1];
        remote_anim[i].body_yaw_init = 1;
    }

    float look_yaw = rp->rot[1];
    float look_pitch = rp->rot[0];
    if (dt > 0.0f) {
        remote_anim[i].body_yaw = skinned_update_body_yaw(remote_anim[i].body_yaw, look_yaw, dt);
    }

    float head_yaw = skinned_head_yaw_offset(remote_anim[i].body_yaw, look_yaw);

    float foot_y = get_feet_align_y(g_ctx.player_walk_model) * sk->scale[1] - 0.5f;
    sk->pos[0] = rp->pos[0];
    sk->pos[1] = rp->pos[1] + foot_y;
    sk->pos[2] = rp->pos[2];
    sk->rot[0] = 0.0f;
    sk->rot[1] = remote_anim[i].body_yaw;
    sk->rot[2] = 0.0f;
    sk->scale[0] = 0.5f;
    sk->scale[1] = 0.5f;
    sk->scale[2] = 0.5f;

    SkinnedLook look = {
        .enabled = 1,
        .head_yaw = head_yaw,
        .head_pitch = look_pitch,
        .head_bone = g_ctx.bone_head,
        .neck_bone = g_ctx.bone_neck,
    };

    Skinned_render_request request = {
        .skinned = sk,
        .skeleton = g_ctx.player_walk_model->skeleton,
        .anim = remote_anim[i].walk_state,
        .look = look,
    };

    program_use(g_ctx.skinned_prog);
    if (!shadow_pass) {
        texture_bind(g_ctx.player_tex, 0);
        texture_bind(g_ctx.player_shininess, 1);
        program_set_int(g_ctx.skinned_prog, "tex", 0);
        program_set_int(g_ctx.skinned_prog, "roug", 1);
    }

    program_set_mat4(g_ctx.skinned_prog, "projection", (float*)proj);
    program_set_mat4(g_ctx.skinned_prog, "view", (float*)view_mat);

    gfx_skinned_render(&request, g_ctx.skinned_prog);
}

void remote_init(void) {
    memset(remote_anim, 0, sizeof(remote_anim));
}

void remote_draw(const mat4 proj, const mat4 view, float dt, int shadow_pass) {
    RemotePlayer* remotes = network_get_remote_players();
    for (int i = 0; i < CLIENT_MAX_REMOTES; i++) {
        if (!remotes[i].active) {
            remote_anim_cleanup(i);
            continue;
        }

        draw_remote_player(i, &remotes[i], dt, proj, view, shadow_pass);
    }
}

void remote_destroy(void) {
    for (int i = 0; i < CLIENT_MAX_REMOTES; i++) {
        remote_anim_cleanup(i);
    }
}