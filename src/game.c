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
#include <GLFW/glfw3.h>

Player player;
HText demo_text;
World world;

Shader a, b;
Program c;

mat4 projection, view;

float last_time = 0.0f;

int wireframe = 0;
float wdelay = 0.0f;

Input input_manager;
Texture texture_atlas;

void game_init() {
    world_init(&world);
    rng_seed(0x11223344AABBCCDD);

    int grid_size = 9;
    for (int x = 0; x < grid_size; x++) {
        for (int z = 0; z < grid_size; z++) {
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

    File vr = file_open("assets/quad/quad.vsh");
    File fr = file_open("assets/quad/quad.fsh");

    texture_create(&texture_atlas, "assets/terrain.png");

    player_init(&player);

    a.type = GL_VERTEX_SHADER;
    b.type = GL_FRAGMENT_SHADER;

    shader_create(&a, vr.data);
    shader_create(&b, fr.data);

    program_create(&c, &a, &b);

    input_init(&input_manager, _win->glwin);

    text_init("assets/text/text.vsh", "assets/text/text.fsh", "assets/text.png");
    text_create(&demo_text, "BLOCKS", 0x5F, 0, 0);
}

void game_tick(float dt) {
    input_update(&input_manager);

    float current_time = (float)glfwGetTime();
    last_time = current_time;

    glm_perspective(glm_rad(60.0f), 800.0f / 600.0f, 0.1f, 1000.0f, projection);

    player_tick(&world, &player, &input_manager, dt);
    player_get_view(&player, view);

    if (input_down(&input_manager, GLFW_KEY_M) && wdelay < 0) {
        wireframe = !wireframe;
        glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);

        wdelay = 0.25f;
    }

    wdelay-=dt;
}

void game_draw() {
    program_use(&c);

    texture_bind(&texture_atlas, 0);
    program_set_int(&c, "tex", 0);
    program_set_mat4(&c, "proj", (float*)projection);
    program_set_mat4(&c, "view", (float*)view);

    world_render(&world, &c);

    text_draw(&demo_text);

    glfwSwapBuffers(_win->glwin);
}

void game_destroy() {
    world_destroy(&world);
}
