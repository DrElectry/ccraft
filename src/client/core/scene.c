#include "core/scene.h"

#include "core/game.h"
#include "core/player.h"
#include "network/network.h"
#include "core/gfx.h"
#include "gl/tex.h"
#include "gl/shader.h"
#include "gl/vao.h"
#include "gl/vbo.h"
#include "gl/ebo.h"
#include "gl/fbo.h"
#include "core/chunk.h"
#include "core/world.h"
#include "core/tile.h"
#include "gui/chat.h"
#include "utils/rand.h"
#include "utils/gltf.h"
#include "utils/obj.h"
#include "sound/sound.h"

#include <float.h>
#include <string.h>
#include <stdlib.h>

extern Player player;
extern Shader a, b, water_vertex, water_fragment, ba, bbb;
extern Program c, water_prog, bc;
extern Shader skinned_program_dummy;

static Skinned_render_request* player_walk_model;
static static Program skinned_prog;

static Skinned_render_request* player_jump_model;
static AnimationClip* walk_clip;
static AnimState* walk_anim;

typedef struct {
    AnimState* walk_state;
    Skinned* skinned;
    float body_yaw;
    int body_yaw_init;
} RemoteAnim;

static RemoteAnim remote_anim[CLIENT_MAX_REMOTES];

static int bone_head = -1;
static int bone_neck = -1;

static uint8_t remote_names_active[CLIENT_MAX_REMOTES] = {0};
static char remote_nick_buf[CLIENT_MAX_REMOTES][MAX_NICKNAME_LEN];
static HText remote_names[CLIENT_MAX_REMOTES];

static vec3 light_pos = { 20.0f, 40.0f, -30.0f };
static vec3 light_dir = { -2.0f, 4.0f, -3.0f };
static vec3 target = { 32.0f, 0.0f, 32.0f };
static vec3 up = { 0.0f, 1.0f, 0.0f };

static Render_request* text;

extern mat4 projection, view, inv_projection, inv_view, light_proj, light_view, light_space_matrix, hand_model;
extern mat4 prev_view_proj;
extern vec3 light_dir;
extern vec3 light_pos;
extern vec3 up;

extern Texture texture_atlas, roughness, brightt, textt, player_tex, player_shininess;
extern Render_request block;
extern Program c, water_prog, bc;
extern Shader a, b, water_vertex, water_fragment, ba, bbb;
extern float aa[64];
extern int bb[64];
extern int cc, dd;
extern Program skinned_prog;
extern mat4 hand_model;
extern vec3 body_pos;
extern Input input_manager;
extern Render_request* text;
extern float last_time;
extern Program bc;

static void remote_anim_cleanup(int i);
static float get_feet_align_y(Skinned_render_request* req);
static void draw_remote_player(int i, const RemotePlayer* rp, float dt, const mat4 proj, const mat4 view_mat);
static void project_point_to_screen(vec3 world_pos, float* out_x, float* out_y);

void scene_init(void) {}
void scene_tick(float dt) {}

static void remote_anim_cleanup(int i) {
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

static float get_feet_align_y(Skinned_render_request* req) {
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

static void draw_remote_player(int i, const RemotePlayer* rp, float dt, const mat4 proj, const mat4 view_mat) {
    if (!rp->active || !player_walk_model || !walk_anim) {
        return;
    }

    int walking = rp->on_ground != 0;

    if (!remote_anim[i].skinned) {
        remote_anim[i].skinned = malloc(sizeof(Skinned));
        memcpy(remote_anim[i].skinned, player_walk_model->skinned, sizeof(Skinned));
        remote_anim[i].skinned->gpu.anim = NULL;
        remote_anim[i].skinned->gpu.skeleton = player_walk_model->skeleton;
    }

    Skinned* sk = remote_anim[i].skinned;
    sk->gpu.skeleton = player_walk_model->skeleton;

    if (!remote_anim[i].walk_state) {
        remote_anim[i].walk_state = anim_state_create(walk_anim->clip);
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

    float foot_y = get_feet_align_y(player_walk_model) * sk->scale[1] - 0.5f;
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
        .head_bone = bone_head,
        .neck_bone = bone_neck,
    };

    Skinned_render_request request = {
        .skinned = sk,
        .skeleton = player_walk_model->skeleton,
        .anim = remote_anim[i].walk_state,
        .look = look,
    };

    program_use(&skinned_prog);
    texture_bind(&player_tex, 0);
    texture_bind(&player_shininess, 1);
    program_set_int(&skinned_prog, "tex", 0);
    program_set_int(&skinned_prog, "roug", 1);
    program_set_mat4(&skinned_prog, "projection", (float*)proj);
    program_set_mat4(&skinned_prog, "view", (float*)view_mat);

    gfx_skinned_render(&request, &skinned_prog);
}

static void project_point_to_screen(vec3 world_pos, float* out_x, float* out_y) {
    vec4 wp = { world_pos[0], world_pos[1], world_pos[2], 1.0f };
    vec4 view_pos;
    vec4 clip;

    glm_mat4_mulv(view, wp, view_pos);
    glm_mat4_mulv(projection, view_pos, clip);

    if (clip[3] == 0.0f) {
        *out_x = 0.0f;
        *out_y = 0.0f;
        return;
    }

    if (view_pos[2] > 0.0f || clip[3] < 0.0f) {
        *out_x = -1.0f;
        *out_y = -1.0f;
        return;
    }

    float ndc_x = clip[0] / clip[3];
    float ndc_y = clip[1] / clip[3];

    *out_x = (ndc_x * 0.5f + 0.5f) * (float)WIDTH;
    *out_y = (0.5f - ndc_y * 0.5f) * (float)HEIGHT;
}

void scene_render_main(float time) {
    float dt = time;
    if (!__onserv) return;

    network_pump();

    RemotePlayer* remotes = network_get_remote_players();
    for (int i = 0; i < CLIENT_MAX_REMOTES; i++) {
        if (!remotes[i].active) {
            remote_anim_cleanup(i);
            continue;
        }
        draw_remote_player(i, &remotes[i], dt, projection, view);
    }
}

void scene_render_shadow(float dt) {
    if (__onserv) {
        network_pump();
    }

    glm_ortho(-32.0f, 32.0f, -32.0f, 32.0f, 1.0f, 200.0f, light_proj);

    vec3 light_offset = { 20.0f, 40.0f, -30.0f };

    float orthoSize = 32.0f;
    float texelSize = (orthoSize * 2.0f) / 2048.0f;

    vec3 snapped_player_pos;
    snapped_player_pos[0] = floorf(player.camera.pos[0]);
    snapped_player_pos[1] = floorf(player.camera.pos[1]+0.01f);
    snapped_player_pos[2] = floorf(player.camera.pos[2]);

    vec3 light_pos_tmp;
    glm_vec3_add(snapped_player_pos, light_offset, light_pos_tmp);

    glm_lookat(light_pos_tmp, snapped_player_pos, up, light_view);

    mat4 light_view_tmp;
    glm_mat4_copy(light_view, light_view_tmp);

    vec4 centerLS;
    glm_mat4_mulv(light_view_tmp, (vec4){ snapped_player_pos[0], snapped_player_pos[1], snapped_player_pos[2], 1.0f }, centerLS);

    centerLS[0] = floorf(centerLS[0] / texelSize) * texelSize;
    centerLS[1] = floorf(centerLS[1] / texelSize) * texelSize;

    vec4 snapped_world;
    mat4 inv_light_view;
    glm_mat4_inv(light_view_tmp, inv_light_view);
    glm_mat4_mulv(inv_light_view, centerLS, snapped_world);

    glm_vec3_add((vec3){ snapped_world[0], snapped_world[1], snapped_world[2] }, light_offset, light_pos_tmp);
    glm_lookat(light_pos_tmp, snapped_player_pos, up, light_view);

    glm_mat4_mul(light_proj, light_view, light_space_matrix);

    glm_vec3_copy(light_offset, light_dir);
    glm_vec3_normalize(light_dir);

    glViewport(0, 0, 2048, 2048);
    glEnable(GL_DEPTH_TEST);
    glClear(GL_DEPTH_BUFFER_BIT);

    if (__onserv) {
        RemotePlayer* remotes = network_get_remote_players();
        for (int i = 0; i < CLIENT_MAX_REMOTES; i++) {
            if (!remotes[i].active) continue;
            draw_remote_player(i, &remotes[i], 0.0f, light_proj, light_view);
        }
    }
}

void scene_render_hud(void) {
    if (!__onserv) return;

    RemotePlayer* remotes = network_get_remote_players();
    for (int i = 0; i < CLIENT_MAX_REMOTES; i++) {
        if (!remotes[i].active) continue;

        if (!remote_names_active[i]) {
            remote_nick_buf[i][0] = '\0';
            memcpy(remote_nick_buf[i], remotes[i].nickname, MAX_NICKNAME_LEN);
            for (int j = 0; remotes[i].nickname[j] && j < MAX_NICKNAME_LEN - 1; j++) {
                char c = remotes[i].nickname[j];
                remote_nick_buf[i][j] = (c >= 'a' && c <= 'z') ? (c - 'a' + 'A') : c;
            }
            remote_nick_buf[i][MAX_NICKNAME_LEN - 1] = '\0';

            text_create(&remote_names[i], remote_nick_buf[i], 0x0F, 0, 0);
            remote_names_active[i] = 1;
        }

        float sx = 0.0f, sy = 0.0f;
        vec3 wp = { remotes[i].pos[0], remotes[i].pos[1] + 1.8f, remotes[i].pos[2] };
        project_point_to_screen(wp, &sx, &sy);

        int text_length = strlen(remote_nick_buf[i]);
        int text_width = text_length * CHAR_WIDTH;
        int text_height = CHAR_HEIGHT;

        remote_names[i].x = (int)sx - (text_width / 2);
        remote_names[i].y = (int)sy - text_height - 4;

        text_create(&remote_names[i], remote_nick_buf[i], 0x0F, remote_names[i].x, remote_names[i].y);
        text_draw(&remote_names[i]);
    }
}

void scene_tick(float dt) {}

void scene_destroy(void) {
    for (int i = 0; i < CLIENT_MAX_REMOTES; i++) {
        remote_anim_cleanup(i);
    }
}

