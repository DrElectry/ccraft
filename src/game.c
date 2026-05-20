#include "game.h"
#include "log.h"
#include "win.h"
#include "glad.h"
#include "shader.h"
#include "fm.h"
#include "player.h"
#include "tex.h"
#include "gfx.h"
#include "chunk.h"
#include "world.h"
#include "input.h"
#include "rand.h"
#include "raycast.h"
#include "fbo.h"
#include "tile.h"
#include "vao.h"
#include "vbo.h"
#include "ebo.h"
#include "sound.h"
#include "obj.h"
#include <GLFW/glfw3.h>
#include <string.h>

Player player;
HText name, fps, pos;
World world;

File world_file;

Shader a, b, water_vertex, water_fragment, ba, bbb;
Program c, water_prog, bc;

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
Texture texture_atlas, roughness, brightt, textt;

Render_request block; // in your hand

static float break_delay = 0.0f;
static float place_delay = 0.0f;

static float g_footstep_delay = 0.0f;

#define SOUND_VARIANTS_PER_PACK 4
#define SOUND_PACK_COUNT 4

static sound_t* g_packs[SOUND_PACK_COUNT][SOUND_VARIANTS_PER_PACK] = {{0}};
static bool g_packs_loaded = false;

static void packs_ensure_loaded(void) {
    if (g_packs_loaded)
        return;

    int pack = 0;
    g_packs[pack][0] = sound_load("assets/sounds/grass1.wav");
    g_packs[pack][1] = sound_load("assets/sounds/grass2.wav");
    g_packs[pack][2] = sound_load("assets/sounds/grass3.wav");
    g_packs[pack][3] = sound_load("assets/sounds/grass4.wav");

    pack = 1;
    g_packs[pack][0] = sound_load("assets/sounds/stone1.wav");
    g_packs[pack][1] = sound_load("assets/sounds/stone2.wav");
    g_packs[pack][2] = sound_load("assets/sounds/stone3.wav");
    g_packs[pack][3] = sound_load("assets/sounds/stone4.wav");

    pack = 2;
    g_packs[pack][0] = sound_load("assets/sounds/gravel1.wav");
    g_packs[pack][1] = sound_load("assets/sounds/gravel2.wav");
    g_packs[pack][2] = sound_load("assets/sounds/gravel3.wav");
    g_packs[pack][3] = sound_load("assets/sounds/gravel4.wav");

    pack = 3;
    g_packs[pack][0] = sound_load("assets/sounds/wood1.wav");
    g_packs[pack][1] = sound_load("assets/sounds/wood2.wav");
    g_packs[pack][2] = sound_load("assets/sounds/wood3.wav");
    g_packs[pack][3] = sound_load("assets/sounds/wood4.wav");

    g_packs_loaded = true;
}

static sound_t* pick_pack_sound(uint16_t tile_id, int variant) {
    uint16_t pack = chunk_sound_pack(tile_id);
    if (pack >= SOUND_PACK_COUNT)
        pack = 0;

    uint16_t v = (uint16_t)(variant & (SOUND_VARIANTS_PER_PACK - 1));
    return g_packs[pack][v];
}

void new_world(const char* filename) {
    File file = file_create(filename);
    
    file_addwrite(&file, "CCRAFT", 6);
    
    uint64_t seed = rng_get_world_seed();
    file_addwrite(&file, (char*)&seed, 8);
    
    float player_data[5] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    file_addwrite(&file, (char*)player_data, 20);
    
    int zero = 0;
    file_addwrite(&file, (char*)&zero, sizeof(int));
}

void game_init() {
    glm_ortho(-64.0f, 64.0f, -64.0f, 64.0f, 1.0f, 200.0f, light_proj);
    glm_lookat(light_pos, target, up, light_view);

    uint64_t seed = 0x0000000000000000;

    if (file_exists("worlds/main.dat")) {
        world_file = file_open("worlds/main.dat");
    } else {
        printf("worlds/main.dat does not exist, creating a new world...\n");
        new_world("worlds/main.dat");
    }

    world_file = file_open("worlds/main.dat");

    memcpy(&seed, world_file.data + 6, 8);
    rng_seed(seed);
    world_init(&world);

    world_load(&world, &world_file);

    texture_atlas.mag_filter = GL_NEAREST;
    texture_atlas.min_filter = GL_NEAREST;
    texture_atlas.wrap_s = GL_REPEAT;
    texture_atlas.wrap_t = GL_REPEAT;

    roughness.mag_filter = GL_NEAREST;
    roughness.min_filter = GL_NEAREST;
    roughness.wrap_s = GL_REPEAT;
    roughness.wrap_t = GL_REPEAT;

    sound_init();

    File vr = file_open("assets/tile/tile.vsh");
    File fr = file_open("assets/tile/tile.fsh");
    File vb = file_open("assets/things/hand.vsh");
    File fb = file_open("assets/things/hand.fsh");
    File water_vr = file_open("assets/tile/tile_water.vsh");
    File water_fr = file_open("assets/tile/tile_water.fsh");

    texture_create(&texture_atlas, "assets/terrain.png");
    texture_create(&roughness, "assets/shiny.png");

    texture_create(&brightt, "assets/brightxt.png");
    texture_create(&textt, "assets/txt.png");

    player_init(&player);

    a.type = GL_VERTEX_SHADER;
    b.type = GL_FRAGMENT_SHADER;
    ba.type = GL_VERTEX_SHADER;
    bbb.type = GL_FRAGMENT_SHADER;
    water_vertex.type = GL_VERTEX_SHADER;
    water_fragment.type = GL_FRAGMENT_SHADER;

    shader_create(&a, vr.data);
    shader_create(&b, fr.data);
    shader_create(&water_vertex, water_vr.data);
    shader_create(&water_fragment, water_fr.data);
    shader_create(&ba, vb.data);
    shader_create(&bbb, fb.data);

    program_create(&c, &a, &b);
    program_create(&bc, &ba, &bbb);
    program_create(&water_prog, &water_vertex, &water_fragment);

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

    text_init("assets/gui/text.vsh", "assets/gui/text.fsh", "assets/text.png");
    text_create(&name, "CCRAFT", 0x0F, 0, 0);
    text_create(&fps, "FPS", 0x0F, 0, 16);
    text_create(&pos, "POS", 0x0F, 0, 32);

    sound_t* ambient;

    ambient = sound_load("assets/sounds/ambient.wav");
    sound_set_looping(ambient, true);
    sound_set_volume(ambient, 0.2f);
    sound_play(ambient);

    sound_t* music;

    music = sound_load("assets/sounds/taswell.wav");
    sound_set_looping(music, true);
    sound_set_volume(music, 0.5f);
    sound_play(music);

    text = obj_load_render_request("assets/text.obj");

    player_get_pos(&player, text->pos);

    text->pos[1]-=20.0f;
    text->pos[0]-=28.0f;
    text->pos[2]+=1.0f;
    text->scale[0]=0.5f;
    text->scale[1]=0.5f;
    text->scale[2]=0.5f;
}

static void rebuild_chunks_for_block(World* world, int wx, int wy, int wz) {
    int cx = (wx >= 0) ? wx / CHUNK_WIDTH : (wx - CHUNK_WIDTH + 1) / CHUNK_WIDTH;
    int cz = (wz >= 0) ? wz / CHUNK_DEPTH : (wz - CHUNK_DEPTH + 1) / CHUNK_DEPTH;

    world_rebuild_chunk(world, cx, cz);

    int lx = wx - cx * CHUNK_WIDTH;
    int lz = wz - cz * CHUNK_DEPTH;

    if (lx == 0) world_rebuild_chunk(world, cx - 1, cz);
    if (lx == CHUNK_WIDTH - 1) world_rebuild_chunk(world, cx + 1, cz);
    if (lz == 0) world_rebuild_chunk(world, cx, cz - 1);
    if (lz == CHUNK_DEPTH - 1) world_rebuild_chunk(world, cx, cz + 1);
}

void game_tick(float dt) {
    glm_mat4_mul(projection, view, prev_view_proj);

    input_update(&input_manager);

    packs_ensure_loaded();

    float current_time = (float)glfwGetTime();

    last_time = current_time;

    glm_perspective(glm_rad(60.0f), 1280.0f / 720.0f, 0.1f, 1000.0f, projection);

    player_tick(&world, &player, &input_manager, dt);
    player_get_view(&player, view);

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

            world_set_block(&world, hit.bx, hit.by, hit.bz, AIR);
            rebuild_chunks_for_block(&world, hit.bx, hit.by, hit.bz);
            break_delay = 0.2f;
        }
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
                uint16_t block = LEAVES;

                int variant = RAND(0, 3);
                sound_t* s = pick_pack_sound(block, variant);
                if (s) {
                    sound_set_looping(s, false);
                    sound_set_volume(s, 1.0f);
                    sound_play(s);
                }

                world_set_block(&world, px, py, pz, block);
                rebuild_chunks_for_block(&world, px, py, pz);
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
}

void game_shadow_pass(void) {
    glm_ortho(-32.0f, 32.0f, -32.0f, 32.0f, 1.0f, 200.0f, light_proj);

    vec3 light_offset = { 20.0f, 40.0f, -30.0f };
    glm_vec3_add(player.camera.pos, light_offset, light_pos);
    glm_lookat(light_pos, player.camera.pos, up, light_view);
    glm_mat4_mul(light_proj, light_view, light_space_matrix);

    glm_vec3_copy(light_offset, light_dir);
    glm_vec3_normalize(light_dir);

    glViewport(0, 0, 4096, 4096);
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

    glDisable(GL_CULL_FACE);

    vao_bind(&block.cache.vao);
    glDrawElements(GL_TRIANGLES, block.tri_count * 3, GL_UNSIGNED_INT, NULL);

    glEnable(GL_CULL_FACE);

    glViewport(0, 0, 1280, 720);
}

void game_draw(float time) {
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

    glDisable(GL_CULL_FACE);
    
    vao_bind(&block.cache.vao);
    glDrawElements(GL_TRIANGLES, block.tri_count * 3, GL_UNSIGNED_INT, NULL);

    glEnable(GL_CULL_FACE);
}


void game_draw_hud() {
    text_draw(&name);
    text_draw(&fps);
    text_draw(&pos);
}

void game_destroy() {
    world_save(&world, "worlds/main.dat");

    if (g_packs_loaded) {
        for (int p = 0; p < SOUND_PACK_COUNT; p++) {
            for (int v = 0; v < SOUND_VARIANTS_PER_PACK; v++) {
                if (g_packs[p][v]) {
                    sound_free(g_packs[p][v]);
                    g_packs[p][v] = NULL;
                }
            }
        }
        g_packs_loaded = false;
    }

    world_destroy(&world);
}


