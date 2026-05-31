#include "core/game.h"
#include "utils/log.h"
#include "core/win.h"
#include "glad.h"
#include "gl/shader.h"
#include "utils/fm.h"
#include "core/player.h"
#include "gl/tex.h"
#include "core/gfx.h"
#include "core/chunk.h"
#include "core/world.h"
#include "core/input.h"
#include "utils/rand.h"
#include "physics/raycast.h"
#include "gl/fbo.h"
#include "core/tile.h"
#include "gl/vao.h"
#include "gl/vbo.h"
#include "gl/ebo.h"
#include "sound/sound.h"
#include "core/main.h"
#include "utils/obj.h"
#include "sound/sound_pack.h"
#include "core/main.h"
#include "network/network.h"
#include "utils/gltf.h"
#include <GLFW/glfw3.h>
#include <string.h>

Player player;
HText name, fps;
World world;

uint16_t blockih = IRON_BLOCK;

File world_file;

Shader a, b, water_vertex, water_fragment, ba, bbb;
Program c, water_prog, bc;

static GLTFModel player_walk_model;
static GLTFModel player_jump_model;
static AnimState* walk_anim;
static AnimState* jump_anim; // used for time stepping only; per-remote playback handled separately
static Program skinned_prog;

typedef struct {
    AnimState* walk_state;
    Skinned* skinned;
    float body_yaw;
    int body_yaw_init;
} RemoteAnim;

static RemoteAnim remote_anim[CLIENT_MAX_REMOTES];

static int bone_head = -1;
static int bone_neck = -1;

vec3 body_pos;

mat4 projection, view, inv_projection, inv_view, light_proj, light_view, light_space_matrix, hand_model;
mat4 prev_view_proj;

Render_request* text; // demo for screenshot

vec3 light_pos = { 20.0f, 40.0f, -30.0f };
vec3 light_dir = { -2.0f, 4.0f, -3.0f };
vec3 target = { 32.0f, 0.0f, 32.0f };
vec3 up = { 0.0f, 1.0f, 0.0f };

float last_time = 0.0f;

int wireframe = 0;
int potato_mode = 0;
int noclip = 0;

float wdelay = 0.0f;
float pdelay = 0.0f;
float ndelay = 0.0f;

float aa[64];
int bb[64];

// shitcode

int cc, dd; // for block in our hand

Input input_manager;
Texture texture_atlas, roughness, brightt, textt, player_tex, player_shininess;

Render_request block; // in your hand

static float break_delay = 0.0f;
static float place_delay = 0.0f;

static float g_footstep_delay = 0.0f;

static uint8_t remote_names_active[CLIENT_MAX_REMOTES] = {0};
static char remote_nick_buf[CLIENT_MAX_REMOTES][MAX_NICKNAME_LEN];
static HText remote_names[CLIENT_MAX_REMOTES];

// PlayerStatePacket.on_ground is used as the walking flag (wasd held) over the network
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

static void draw_remote_player(int i, const RemotePlayer* rp, float dt, const mat4 proj, const mat4 view_mat) {
    if (!rp->active || !player_walk_model.skinned || !walk_anim) {
        return;
    }

    int walking = rp->on_ground != 0;

    if (!remote_anim[i].skinned) {
        remote_anim[i].skinned = malloc(sizeof(Skinned));
        *remote_anim[i].skinned = *player_walk_model.skinned;
        remote_anim[i].skinned->gpu.anim = NULL;
    }

    Skinned* sk = remote_anim[i].skinned;
    sk->gpu.skeleton = player_walk_model.skeleton;

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

    float foot_y = player_walk_model.feet_align_y * sk->scale[1]-0.5f;
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

    program_use(&skinned_prog);
    texture_bind(&player_tex, 0);
    texture_bind(&player_shininess, 1);
    program_set_int(&skinned_prog, "tex", 0);
    program_set_int(&skinned_prog, "roug", 1);
    program_set_mat4(&skinned_prog, "projection", (float*)proj);
    program_set_mat4(&skinned_prog, "view", (float*)view_mat);

    skinned_render(sk, &skinned_prog, 0.0f, &look);
}

void game_init() {
    glm_ortho(-64.0f, 64.0f, -64.0f, 64.0f, 1.0f, 200.0f, light_proj);
    glm_lookat(light_pos, target, up, light_view);
    
    uint64_t seed = 0x0000000000000000;
    rng_seed(seed);

    if (__onserv) {
        rng_seed(__servseed);
    } else {
        if (file_exists("worlds/main.dat")) {
            world_file = file_open("worlds/main.dat");
        } else {
            printf("worlds/main.dat does not exist, creating a new world...\n");
            new_world("worlds/main.dat");
        }

        world_file = file_open("worlds/main.dat");

        memcpy(&seed, world_file.data + 6, 8);

        rng_seed(seed);
    }
    world_init(&world);

    if (!__onserv) {
        world_load(&world, &world_file);
    }

    texture_atlas.mag_filter = GL_NEAREST;
    texture_atlas.min_filter = GL_NEAREST;
    texture_atlas.wrap_s = GL_REPEAT;
    texture_atlas.wrap_t = GL_REPEAT;

    roughness.mag_filter = GL_NEAREST;
    roughness.min_filter = GL_NEAREST;
    roughness.wrap_s = GL_REPEAT;
    roughness.wrap_t = GL_REPEAT;

    sound_init();

    texture_create(&texture_atlas, "assets/textures/terrain.png");
    texture_create(&roughness, "assets/textures/shiny.png");
    
    texture_create(&player_tex, "assets/textures/player.png");
    texture_create(&player_shininess, "assets/textures/player.png");

    texture_create(&brightt, "assets/textures/txt_shininess.png");
    texture_create(&textt, "assets/textures/txt.png");

    player_init(&player);

    gfx_program_create(&skinned_prog, "assets/things/skin.vsh", "assets/things/skin.fsh");
    skinned_program_init(&skinned_prog);

    gfx_program_create(&c, "assets/tile/tile.vsh", "assets/tile/tile.fsh");
    gfx_program_create(&bc, "assets/things/hand.vsh", "assets/things/hand.fsh");
    gfx_program_create(&water_prog, "assets/tile/tile_water.vsh", "assets/tile/tile_water.fsh");

    input_init(&input_manager, _win->glwin);

    tile_push_face(aa, bb, (vec3){0.0f, 0.0f, 0.0f}, &cc, &dd, FRONT, 0);
    tile_push_face(aa, bb, (vec3){0.0f, 0.0f, 0.0f}, &cc, &dd, BACK, 0);
    tile_push_face(aa, bb, (vec3){0.0f, 0.0f, 0.0f}, &cc, &dd, LEFT, 0);
    tile_push_face(aa, bb, (vec3){0.0f, 0.0f, 0.0f}, &cc, &dd, RIGHT, 0);
    tile_push_face(aa, bb, (vec3){0.0f, 0.0f, 0.0f}, &cc, &dd, UP, 0);
    tile_push_face(aa, bb, (vec3){0.0f, 0.0f, 0.0f}, &cc, &dd, DOWN, 0);

    block.data = aa;
    block.data_size = cc*sizeof(float);
    block.triangles = bb;
    block.tri_count = dd;

    gfx_packet_static_request(&block);

    block.pos[0] = block.pos[1] = block.pos[2] = 0.0f;
    block.rot[0] = block.rot[1] = block.rot[2] = 0.0f;
    block.scale[0] = block.scale[1] = block.scale[2] = 0.3f;

    glm_mat4_identity(hand_model);
    glm_translate(hand_model, (vec3){50.0f, 50.0f, 50.0f});

    text_init("assets/gui/text.vsh", "assets/gui/text.fsh", "assets/textures/text.png");
    text_create(&name, "0.30", 0x0F, 0, 0);
    text_create(&fps, "FPS: 0", 0x0F, 0, 16);

    player_get_pos(&player, body_pos);

    sound_t* ambient = sound_load("assets/sounds/ambient.wav");
    sound_set_looping(ambient, true);
    sound_set_volume(ambient, 0.2f);
    sound_play(ambient);

    sound_t* music = sound_load("assets/sounds/taswell.wav");
    sound_set_looping(music, true);
    sound_set_volume(music, 0.5f);
    sound_play(music);

    text = obj_load_render_request("assets/models/text.obj");

    player_get_pos(&player, text->pos);

    text->pos[1]-=20.0f;
    text->pos[0]-=28.0f;
    text->pos[2]+=1.0f;
    text->scale[0]=0.5f;
    text->scale[1]=0.5f;
    text->scale[2]=0.5f;

    if (!gltf_load("assets/models/player_walk.glb", &player_walk_model)) {
        printf("Failed to load walk.glb\n");
        return;
    }
    
    for (int i = 0; i < player_walk_model.animation_count; i++) {
        printf("Anim %d: %s\n", i, player_walk_model.animation_names[i]);
    }
    
    AnimationClip* walk = gltf_get_animation(&player_walk_model, "mixamo.com");
    if (!walk && player_walk_model.animation_count > 0) {
        walk = player_walk_model.animations[0];
    }
    
    if (walk) {
        walk_anim = anim_state_create(walk);
        walk_anim->loop = 1;
        walk_anim->speed = 1.0f;
        player_walk_model.skinned->gpu.skeleton = player_walk_model.skeleton;
    }

    if (player_walk_model.skeleton) {
        bone_head = skeleton_find_bone(player_walk_model.skeleton, "mixamorig:Head");
        bone_neck = skeleton_find_bone(player_walk_model.skeleton, "mixamorig:Neck");
    }
}

void game_tick(float dt) {
    glm_mat4_mul(projection, view, prev_view_proj);

    packs_ensure_loaded();

    glfwPollEvents();
    input_update(&input_manager);

    if (__onserv) {
        network_pump();
    }

    static double fps_accum_time = 0.0;
    static int fps_frames = 0;
    static float current_fps = 0.0f;

    fps_accum_time += (double)dt;
    fps_frames++;
    if (fps_accum_time >= 0.25) {
        current_fps = (float)fps_frames / (float)fps_accum_time;
        fps_accum_time = 0.0;
        fps_frames = 0;
    }

    static char fps_text[32];
    snprintf(fps_text, sizeof(fps_text), "FPS: %.0f", current_fps);
    text_free(&fps);
    text_create(&fps, fps_text, 0x0F, 0, 16);
    text_create(&name, "0.30", 0x0F, 0, 0);

    glm_perspective(glm_rad(60.0f), ((float)WIDTH) / ((float)HEIGHT), 0.1f, 1000.0f, projection);

    player_tick(&world, &player, &input_manager, dt);
    player_get_view(&player, view);

    if (__onserv) {
        static float last_send = 0.0f;
        float now = (float)glfwGetTime();
        if (now - last_send >= 1.0f / UPDATE_RATE) {
            vec3 player_pos;
            player_get_pos(&player, player_pos);
            uint8_t walking = (input_down(&input_manager, GLFW_KEY_W) || input_down(&input_manager, GLFW_KEY_A) || input_down(&input_manager, GLFW_KEY_S) || input_down(&input_manager, GLFW_KEY_D)) ? 1 : 0;
            network_send_player_state(
                network_get_local_client_id(),
                player_pos,
                player.camera.rot,
                walking
            );
            last_send = now;
        }
    }

    glm_mat4_inv(projection, inv_projection);
    glm_mat4_inv(view, inv_view);

    if (input_down(&input_manager, GLFW_KEY_M) && wdelay < 0) {
        wireframe = !wireframe;
        glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
        wdelay = 0.25f;
    }
    if (input_down(&input_manager, GLFW_KEY_N) && pdelay < 0) {
        potato_mode = !potato_mode;
        pdelay = 0.25f;
    }
    if (input_down(&input_manager, GLFW_KEY_B) && ndelay < 0) {
        noclip = !noclip;
        player_set_noclip(&player, noclip);
        ndelay = 0.25f;
    }

    wdelay -= dt;
    pdelay -= dt;
    ndelay -= dt;

    vec3 eye;
    player_get_eye(&player, eye);

    world_tick(&world, eye);

    RaycastHit hit;
    raycast_dda(&world, eye, player.camera.forward, 5.0f, &hit);

    break_delay -= dt;
    place_delay -= dt;
    g_footstep_delay -= dt;

    text->rot[1]+=dt;

    if (hit.hit && break_delay <= 0.0f) {
        if (glfwGetMouseButton(input_manager.win, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
            uint16_t b = world_get_block(&world, hit.bx, hit.by, hit.bz);
            int variant = RAND(0, 3);
            sound_t* s = pick_pack_sound(b, variant);
            if (s) {
                sound_set_looping(s, false);
                sound_set_volume(s, 1.0f);
                sound_play(s);
            }

            if (!__onserv) {
                world_set_block(&world, hit.bx, hit.by, hit.bz, AIR);
                rebuild_chunks_for_block(&world, hit.bx, hit.by, hit.bz);
            } else {
                network_send_block_change(network_get_local_client_id(), hit.bx, hit.by, hit.bz, AIR);
            }
            break_delay = 0.2f;
        }
    }

    if (input_manager.scroll_y != 0.0f) {
        int dir = (input_manager.scroll_y > 0.0f) ? 1 : -1;
        int next = (int)blockih + dir;
        if (next < FIRST_TILE) next = LAST_TILE;
        if (next > LAST_TILE) next = FIRST_TILE;
        blockih = (uint16_t)next;
        input_manager.scroll_y = 0.0f;
        place_delay = 0.0f;
    }

    if (hit.hit && place_delay <= 0.0f) {
        if (glfwGetMouseButton(input_manager.win, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
            int px = hit.bx + (int)hit.normal[0];
            int py = hit.by + (int)hit.normal[1];
            int pz = hit.bz + (int)hit.normal[2];


            vec3 player_pos;
            player_get_pos(&player, player_pos);
            
            float player_min_x = player_pos[0] - 0.3f;
            float player_max_x = player_pos[0] + 0.3f;
            float player_min_y = player_pos[1];
            float player_max_y = player_pos[1] + 1.8f;
            float player_min_z = player_pos[2] - 0.3f;
            float player_max_z = player_pos[2] + 0.3f;
            
            int collision = (px + 1 > player_min_x && px < player_max_x) &&
                            (py + 1 > player_min_y && py < player_max_y) &&
                            (pz + 1 > player_min_z && pz < player_max_z);
            
            if (!collision) {
                int variant = RAND(0, 3);
                sound_t* s = pick_pack_sound(blockih, variant);
                if (s) {
                    sound_set_looping(s, false);
                    sound_set_volume(s, 1.0f);
                    sound_play(s);
                }

                if (!__onserv) {
                    world_set_block(&world, px, py, pz, blockih);
                    rebuild_chunks_for_block(&world, px, py, pz);
                } else {
                    network_send_block_change(network_get_local_client_id(), px, py, pz, blockih);
                }
                place_delay = 0.2f;
            }
        }
    }

    if (g_footstep_delay <= 0.0f) {
        vec3 ppos;
        player_get_pos(&player, ppos);

        int wx = (int)floorf(ppos[0]);
        int wz = (int)floorf(ppos[2]);
        int wy = (int)floorf(ppos[1]);

        uint16_t below = world_get_block(&world, wx, wy, wz);
        if (below != AIR) {
            float move_x = input_down(&input_manager, GLFW_KEY_W) || input_down(&input_manager, GLFW_KEY_S) ? 1.0f : 0.0f;
            float move_z = input_down(&input_manager, GLFW_KEY_A) || input_down(&input_manager, GLFW_KEY_D) ? 1.0f : 0.0f;
            if (move_x + move_z > 0.5f) {
                int variant = RAND(0, 3);
                sound_t* s = pick_pack_sound(below, variant);
                if (s)
                    sound_play(s);

                g_footstep_delay = 0.25f;
            }

        }
    }

    network_update_remotes(dt);
}

void game_shadow_pass(void) {
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

    vec3 light_pos;
    glm_vec3_add(snapped_player_pos, light_offset, light_pos);

    glm_lookat(light_pos, snapped_player_pos, up, light_view);

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

    glm_vec3_add((vec3){ snapped_world[0], snapped_world[1], snapped_world[2] }, light_offset, light_pos);
    glm_lookat(light_pos, snapped_player_pos, up, light_view);

    glm_mat4_mul(light_proj, light_view, light_space_matrix);

    glm_vec3_copy(light_offset, light_dir);
    glm_vec3_normalize(light_dir);

    glViewport(0, 0, 2048, 2048); // 4k ultra rtx shadows
    glEnable(GL_DEPTH_TEST);
    glClear(GL_DEPTH_BUFFER_BIT);
    program_use(&c);
    texture_bind(&texture_atlas, 0);
    texture_bind(&roughness, 1);
    program_set_int(&c, "tex", 0);
    program_set_int(&c, "roug", 1);
    program_set_mat4(&c, "proj", (float*)light_proj);
    program_set_mat4(&c, "view", (float*)light_view);

    program_use(&water_prog);
    texture_bind(&texture_atlas, 0);
    texture_bind(&roughness, 1);
    program_set_int(&water_prog, "tex", 0);
    program_set_int(&water_prog, "roug", 1);
    program_set_mat4(&water_prog, "proj", (float*)light_proj);
    program_set_mat4(&water_prog, "view", (float*)light_view);

    world_render(&world, &c, &water_prog);

    program_use(&c);

    texture_bind(&textt, 0);
    texture_bind(&brightt, 1);
    gfx_render(text, &c);

    if (__onserv) {
        network_pump();

        RemotePlayer* remotes = network_get_remote_players();
        for (int i = 0; i < CLIENT_MAX_REMOTES; i++) {
            if (!remotes[i].active) continue;
            draw_remote_player(i, &remotes[i], 0.0f, light_proj, light_view);
        }
    }

    vec3 eye;
    player_get_eye(&player, eye);

    glm_mat4_identity(hand_model);

    vec3 fwd;
    vec3 right;
    vec3 upv;
    glm_vec3_copy(player.camera.forward, fwd);
    glm_vec3_copy(player.camera.right, right);
    glm_vec3_copy(player.camera.up, upv);

    float d = 0.7f;
    float x = 0.3f;
    float y = -0.5f;

    vec3 hold_pos;
    glm_vec3_scale(fwd, d, hold_pos);
    glm_vec3_muladds(right, x, hold_pos);
    glm_vec3_muladds(upv, y, hold_pos);
    glm_vec3_add(hold_pos, eye, hold_pos);

    glm_translate(hand_model, hold_pos);

    mat4 rot;
    glm_mat4_identity(rot);
    rot[0][0] = right[0];
    rot[0][1] = right[1];
    rot[0][2] = right[2];
    rot[1][0] = upv[0];
    rot[1][1] = upv[1];
    rot[1][2] = upv[2];
    rot[2][0] = fwd[0];
    rot[2][1] = fwd[1];
    rot[2][2] = fwd[2];

    glm_mat4_mul(hand_model, rot, hand_model);

    glm_scale(hand_model, block.scale);

    program_use(&bc);
    texture_bind(&texture_atlas, 0);
    texture_bind(&roughness, 1);
    program_set_int(&bc, "tex", 0);
    program_set_int(&bc, "roug", 1);
    program_set_mat4(&bc, "proj", (float*)projection);
    program_set_mat4(&bc, "view", (float*)view);
    program_set_mat4(&bc, "model", (float*)hand_model);

    program_set_uint(&bc, "id", lookup_atlas[blockih*6]);

    glDisable(GL_CULL_FACE);

    vao_bind(&block.cache.vao);
    glDrawElements(GL_TRIANGLES, block.tri_count * 3, GL_UNSIGNED_INT, NULL);

    glEnable(GL_CULL_FACE);

    glViewport(0, 0, WIDTH, HEIGHT);
}

void game_draw(float time) {
    float dt = time - last_time;
    last_time = time;
    if (dt <= 0.0f) dt = 0.016f;

    program_use(&c);
    texture_bind(&texture_atlas, 0);
    texture_bind(&roughness, 1);
    program_set_int(&c, "tex", 0);
    program_set_int(&c, "roug", 1);
    program_set_mat4(&c, "proj", (float*)projection);
    program_set_mat4(&c, "view", (float*)view);

    program_use(&water_prog);
    texture_bind(&texture_atlas, 0);
    texture_bind(&roughness, 1);
    program_set_int(&water_prog, "tex", 0);
    program_set_int(&water_prog, "roug", 1);
    program_set_mat4(&water_prog, "proj", (float*)projection);
    program_set_mat4(&water_prog, "view", (float*)view);
    program_set_float(&water_prog, "time", time);

    world_render(&world, &c, &water_prog);

    program_use(&c);

    if (__onserv) {
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

    texture_bind(&textt, 0);
    texture_bind(&brightt, 1);
    gfx_render(text, &c);

    vec3 eye;
    player_get_eye(&player, eye);

    glm_mat4_identity(hand_model);

    vec3 fwd;
    vec3 right;
    vec3 upv;
    glm_vec3_copy(player.camera.forward, fwd);
    glm_vec3_copy(player.camera.right, right);
    glm_vec3_copy(player.camera.up, upv);

    float d = 0.55f;
    float x = 0.2f;
    float y = -0.6f;

    vec3 hold_pos;
    glm_vec3_scale(fwd, d, hold_pos);
    glm_vec3_muladds(right, x, hold_pos);
    glm_vec3_muladds(upv, y, hold_pos);
    glm_vec3_add(hold_pos, eye, hold_pos);

    glm_translate(hand_model, hold_pos);

    mat4 rot;
    glm_mat4_identity(rot);
    rot[0][0] = right[0];
    rot[0][1] = right[1];
    rot[0][2] = right[2];
    rot[1][0] = upv[0];
    rot[1][1] = upv[1];
    rot[1][2] = upv[2];
    rot[2][0] = fwd[0];
    rot[2][1] = fwd[1];
    rot[2][2] = fwd[2];

    glm_mat4_mul(hand_model, rot, hand_model);

    glm_scale(hand_model, block.scale);

    program_use(&bc);
    texture_bind(&texture_atlas, 0);
    texture_bind(&roughness, 1);
    program_set_int(&bc, "tex", 0);
    program_set_int(&bc, "roug", 1);
    program_set_mat4(&bc, "proj", (float*)projection);
    program_set_mat4(&bc, "view", (float*)view);
    program_set_mat4(&bc, "model", (float*)hand_model);
    program_set_uint(&bc, "id", lookup_atlas[blockih*6]);

    glDisable(GL_CULL_FACE);
    
    vao_bind(&block.cache.vao);
    glDrawElements(GL_TRIANGLES, block.tri_count * 3, GL_UNSIGNED_INT, NULL);

    glEnable(GL_CULL_FACE);
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

    // behind the screen
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

void game_draw_hud() {
    text_draw(&name);
    text_draw(&fps);

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

void game_destroy() {
    if (!__onserv) {
        world_save(&world, "worlds/main.dat");
    }
    
    for (int i = 0; i < CLIENT_MAX_REMOTES; i++) {
        remote_anim_cleanup(i);
    }
    
    sound_pack_destroy();
    world_destroy(&world);

    if (walk_anim) {
        anim_state_destroy(walk_anim);
        walk_anim = NULL;
    }
    if (player_walk_model.skinned) {
        player_walk_model.skinned->gpu.anim = NULL;
    }
    gltf_free(&player_walk_model);
    gltf_free(&player_jump_model);
}
