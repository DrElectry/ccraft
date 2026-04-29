#include "log.h"
#include "win.h"
#include "glad.h"
#include "game.h"
#include "gfx.h"
#include "fbo.h"
#include "fm.h"
#include <GLFW/glfw3.h>
#include <cglm/cglm.h>

int main() {
    ASSERT(glfwInit(), "no glfw");

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_DEPTH_BITS, 24);

    Window packet;

    packet.width = 1280;
    packet.height = 720;
    packet.title = "blocks";

    window_create(&packet);

    ASSERT(gladLoadGLLoader((GLADloadproc)glfwGetProcAddress), "no glad");

    glfwSwapInterval(0); // damp this for vsync or no vsync

    glfwSetInputMode(packet.glwin, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
    glfwSetInputMode(packet.glwin, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glClearColor(0.1f, 0.2f, 0.2f, 1.0f);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    FBO gbuffer;
    FBO shadow_pass;

    Shader mainv, mainf;
    Program main;
    File mainfv, mainff;

    mainv.type = GL_VERTEX_SHADER;
    mainf.type = GL_FRAGMENT_SHADER;

    mainfv = file_open("assets/deferred/main.vsh");
    mainff = file_open("assets/deferred/main.fsh");

    shader_create(&mainv, mainfv.data);
    shader_create(&mainf, mainff.data); // i know that writing this in main.c is not the best thing to do, why do i even write so much shit in main.c?

    program_create(&main, &mainv, &mainf);

    fbo_create(&gbuffer, 1280, 720, 2);
    fbo_create_depth(&shadow_pass, 2048, 2048);

    game_init();

    double last_time = glfwGetTime();
    float fps_timer = 0.0f;
    int fps_counter = 0;

    while (!window_shouldclose()) {
        double current_time = glfwGetTime();
        float delta_time = (float)(current_time - last_time);
        last_time = current_time;

        fps_timer += delta_time;
        fps_counter++;

        if (fps_timer >= 1.0f) {
            printf("FPS: %d\n", fps_counter);
            fps_counter = 0;
            fps_timer = 0.0f;
        }

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);

        fbo_bind(&gbuffer);

        window_update();
        window_draw(); // dead code xd

        game_tick(delta_time);
        game_draw();

        fbo_unbind();

        fbo_bind(&shadow_pass);

        window_draw();
        game_shadow_pass();

        fbo_unbind();

        program_use(&main);

        fbo_bind_texture(&gbuffer, 0, 0);
        fbo_bind_texture(&gbuffer, 1, 1);
        fbo_bind_depth_texture(&gbuffer, 2);
        fbo_bind_depth_texture(&shadow_pass, 3);

        program_set_int(&main, "gAlbedo", 0);
        program_set_int(&main, "gNormal", 1);
        program_set_int(&main, "gDepth", 2);
        program_set_int(&main, "dShadow", 3);

        program_set_mat4(&main, "light_space_matrix", (float*)light_space_matrix);
        program_set_mat4(&main, "inv_projection", (float*)inv_projection);
        program_set_mat4(&main, "inv_view", (float*)inv_view);

        program_set_vec3(&main, "lightDir", (float*)light_pos);
        program_set_vec3(&main, "lightColor", (float*)(vec3){2.0f, 2.0f, 2.0f});

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);

        gfx_draw_fullscreen_quad();

        glfwSwapBuffers(_win->glwin);
    }

    game_destroy();

    glfwTerminate();

    return 0;
}