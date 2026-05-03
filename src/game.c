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
#include <GLFW/glfw3.h>

Player player;
HText demo_text;
World world;

Shader a, b, water_vertex, water_fragment;
Program c, water_prog;

mat4 projection, view, inv_projection, inv_view, light_proj, light_view, light_space_matrix;
mat4 prev_view_proj;

vec3 light_pos = { 20.0f, 40.0f, -30.0f };
vec3 light_dir = { 2.0f, 4.0f, -3.0f };
vec3 target = { 32.0f, 0.0f, 32.0f };
vec3 up = { 0.0f, 1.0f, 0.0f };

float last_time = 0.0f;

int wireframe = 0;
float wdelay = 0.0f;

Input input_manager;
Texture texture_atlas, roughness;

Render_request block; // in your hand

static float break_delay = 0.0f;
static float place_delay = 0.0f;
// the quality of this code is the new level of shit

void game_init() {
    glm_ortho(-64.0f, 64.0f, -64.0f, 64.0f, 1.0f, 200.0f, light_proj);
    glm_lookat(light_pos, target, up, light_view);

    world_init(&world);
    rng_seed(0x11223344AABBCCDD);

    int grid_size = 9;

    for (int x = 0; x < grid_size; x++) {
        for (int z = 0; z < grid_size; z++) {
            // Set chunk position for seeded noise generation
            chunk_set_position(x, z);
            
            Chunk chunk;
            chunk_generate(&chunk);
            vec2 pos = { (float)x, (float)z };
            world_add_chunk(&world, &chunk, pos);
        }
    }

    for (int x = 0; x < grid_size; x++) {
        for (int z = 0; z < grid_size; z++) {
            world_rebuild_chunk(&world, x, z);
        }
    }

    texture_atlas.mag_filter = GL_NEAREST;
    texture_atlas.min_filter = GL_NEAREST;
    texture_atlas.wrap_s = GL_REPEAT;
    texture_atlas.wrap_t = GL_REPEAT;

    roughness.mag_filter = GL_NEAREST;
    roughness.min_filter = GL_NEAREST;
    roughness.wrap_s = GL_REPEAT;
    roughness.wrap_t = GL_REPEAT;

    File vr = file_open("assets/tile/tile.vsh");
    File fr = file_open("assets/tile/tile.fsh");
    File water_vr = file_open("assets/tile/tile_water.vsh");
    File water_fr = file_open("assets/tile/tile_water.fsh");

    texture_create(&texture_atlas, "assets/terrain.png");
    texture_create(&roughness, "assets/shiny.png");

    player_init(&player);

    a.type = GL_VERTEX_SHADER;
    b.type = GL_FRAGMENT_SHADER;
    water_vertex.type = GL_VERTEX_SHADER;
    water_fragment.type = GL_FRAGMENT_SHADER;

    shader_create(&a, vr.data);
    shader_create(&b, fr.data);
    shader_create(&water_vertex, water_vr.data);
    shader_create(&water_fragment, water_fr.data);

    program_create(&c, &a, &b);
    program_create(&water_prog, &water_vertex, &water_fragment);

    input_init(&input_manager, _win->glwin);

    text_init("assets/text/text.vsh", "assets/text/text.fsh", "assets/text.png");
    text_create(&demo_text, "CCRAFT", 0x5F, 0, 0);
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

    wdelay -= dt;

    vec3 eye;
    player_get_eye(&player, eye);

    world_tick(&world, eye);

    RaycastHit hit;
    raycast_dda(&world, eye, player.camera.forward, 5.0f, &hit);

    break_delay -= dt;
    place_delay -= dt;

    if (hit.hit && break_delay <= 0.0f) {
        if (glfwGetMouseButton(input_manager.win, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
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

            world_set_block(&world, px, py, pz, GLASS);
            rebuild_chunks_for_block(&world, px, py, pz);
            place_delay = 0.2f;
        }
    }
}

void game_shadow_pass(void) {
    glm_ortho(-64.0f, 64.0f, -64.0f, 64.0f, 1.0f, 200.0f, light_proj);

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

    // Set uniforms for water program too
    program_use(&water_prog);
    texture_bind(&texture_atlas, 0);
    texture_bind(&roughness, 1);
    program_set_int(&water_prog, "tex", 0);
    program_set_int(&water_prog, "roug", 1);
    program_set_mat4(&water_prog, "proj", (float*)projection);
    program_set_mat4(&water_prog, "view", (float*)view);
    program_set_float(&water_prog, "time", time);

    world_render(&world, &c, &water_prog);
}

void game_draw_hud() {
    text_draw(&demo_text);
}

void game_destroy() {
    world_destroy(&world);
}