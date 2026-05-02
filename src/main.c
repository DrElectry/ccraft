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

    glfwSwapInterval(0);

    glfwSetInputMode(packet.glwin, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
    glfwSetInputMode(packet.glwin, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glClearColor(0.3f, 1.0f, 1.0f, 1.0f);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    FBO gbuffer;
    FBO shadow_pass;
    FBO ssaofb;
    FBO ppfb;
    FBO prev_frame;
    FBO ssrfb;
    FBO bloombfb;
    FBO bloomblfb;

    Shader mainv, mainf, ssaof, ppf, ssrf, bloombf, bloomblf;
    Program main, ssao, pp, ssr, bloomb, bloombl;
    File mainfv, mainff, ssaoff, ppff, ssrff, bloombff, bloomblff;

    mainv.type = GL_VERTEX_SHADER;
    mainf.type = GL_FRAGMENT_SHADER;
    ssaof.type = GL_FRAGMENT_SHADER;
    ppf.type = GL_FRAGMENT_SHADER;
    ssrf.type = GL_FRAGMENT_SHADER;
    bloombf.type = GL_FRAGMENT_SHADER;
    bloomblf.type = GL_FRAGMENT_SHADER;

    mainfv = file_open("assets/deferred/main.vsh");
    mainff = file_open("assets/deferred/main.fsh");
    ssaoff = file_open("assets/deferred/ssao.fsh");
    ppff = file_open("assets/deferred/pp.fsh");
    ssrff = file_open("assets/deferred/ssr.fsh");
    bloombff = file_open("assets/deferred/bloomb.fsh");
    bloomblff = file_open("assets/deferred/bloombl.fsh");

    shader_create(&mainv, mainfv.data);
    shader_create(&mainf, mainff.data);
    shader_create(&ssaof, ssaoff.data);
    shader_create(&ppf, ppff.data);
    shader_create(&ssrf, ssrff.data);
    shader_create(&bloombf, bloombff.data);
    shader_create(&bloomblf, bloomblff.data);

    program_create(&main, &mainv, &mainf);
    program_create(&ssao, &mainv, &ssaof);
    program_create(&pp, &mainv, &ppf);
    program_create(&ssr, &mainv, &ssrf);
    program_create(&bloomb, &mainv, &bloombf);
    program_create(&bloombl, &mainv, &bloomblf);

    fbo_create(&ssaofb, 1280, 720, 1);
    fbo_create(&prev_frame, 1280, 720, 1);
    fbo_create(&ppfb, 1280, 720, 1);
    fbo_create(&ssrfb, 1280, 720, 1);
    fbo_create(&bloombfb, 1280, 720, 1);
    fbo_create(&bloomblfb, 1280, 720, 1);

    ssrfb.color_formats[0] = FBO_COLOR_RGBA16F;
    bloombfb.color_formats[0] = FBO_COLOR_RGBA16F;
    bloomblfb.color_formats[0] = FBO_COLOR_RGBA16F;

    fbo_create(&gbuffer, 1280, 720, 5);

    gbuffer.color_formats[0] = FBO_COLOR_RGB16F;
    gbuffer.color_formats[1] = FBO_COLOR_RGB16F;
    gbuffer.color_formats[2] = FBO_COLOR_RG16F;
    gbuffer.color_formats[3] = FBO_COLOR_RGBA16F;
    gbuffer.color_formats[4] = FBO_COLOR_RG16F;

    fbo_create_depth(&shadow_pass, 4096, 4096);

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

        game_tick(delta_time);

        if (wireframe) {
            fbo_unbind();

            window_update();
            window_draw();

            glEnable(GL_DEPTH_TEST);
            glEnable(GL_CULL_FACE);

            game_draw(current_time);

            game_draw_hud();

            glfwSwapBuffers(_win->glwin);
            continue;
        }

        fbo_bind(&prev_frame);

        gfx_draw_fullscreen_quad();

        fbo_unbind();

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);

        fbo_bind(&gbuffer);

        window_update();
        window_draw();

        game_draw(current_time);

        fbo_unbind();

        fbo_bind(&shadow_pass);

        window_draw();

        game_shadow_pass();

        fbo_unbind();

        fbo_bind(&ssaofb);

        window_draw();

        program_use(&ssao);

        fbo_bind_depth_texture(&gbuffer, 0);
        fbo_bind_texture(&gbuffer, 1, 1);

        program_set_int(&ssao, "gDepth", 0);
        program_set_int(&ssao, "gNormal", 1);

        program_set_mat4(&ssao, "inverseProjection", (float*)inv_projection);
        program_set_mat4(&ssao, "proj", (float*)projection);

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);

        gfx_draw_fullscreen_quad();

        fbo_unbind();

        fbo_bind(&ssrfb);

        program_use(&ssr);

        fbo_bind_texture(&gbuffer, 0, 0);
        fbo_bind_texture(&gbuffer, 1, 1);
        fbo_bind_texture(&gbuffer, 3, 3);
        fbo_bind_texture(&gbuffer, 4, 4);

        program_set_int(&ssr, "gAlbedo", 0);
        program_set_int(&ssr, "gNormal", 1);
        program_set_int(&ssr, "gPosition", 3);
        program_set_int(&ssr, "gRoughness", 4);

        program_set_mat4(&ssr, "projection", (float*)projection);
        program_set_mat4(&ssr, "view", (float*)view);

        gfx_draw_fullscreen_quad();

        fbo_unbind();

        fbo_bind(&ppfb);

        program_use(&main);

        fbo_bind_texture(&gbuffer, 0, 0);
        fbo_bind_texture(&gbuffer, 1, 1);
        fbo_bind_depth_texture(&gbuffer, 2);
        fbo_bind_depth_texture(&shadow_pass, 3);
        fbo_bind_texture(&ssaofb, 0, 4);
        fbo_bind_texture(&ssrfb, 0, 5);

        program_set_int(&main, "gAlbedo", 0);
        program_set_int(&main, "gNormal", 1);
        program_set_int(&main, "gDepth", 2);
        program_set_int(&main, "dShadow", 3);
        program_set_int(&main, "dSSAO", 4);
        program_set_int(&main, "dSSR", 5);

        program_set_mat4(&main, "light_space_matrix", (float*)light_space_matrix);
        program_set_mat4(&main, "inv_projection", (float*)inv_projection);
        program_set_mat4(&main, "inv_view", (float*)inv_view);

        program_set_vec3(&main, "lightDir", (float*)light_dir);
        program_set_vec3(&main, "lightColor", (float*)(vec3){1.0f, 1.0f, 1.0f});

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);

        gfx_draw_fullscreen_quad();

        fbo_unbind();

        fbo_bind(&bloombfb);

        program_use(&bloomb);

        fbo_bind_texture(&ppfb, 0, 0);

        program_set_int(&bloomb, "sceneTex", 0);

        gfx_draw_fullscreen_quad();

        fbo_unbind();

        fbo_bind(&bloomblfb);

        program_use(&bloombl);

        fbo_bind_texture(&bloombfb, 0, 0);

        program_set_int(&bloombl, "image", 0);
        program_set_vec2(&bloombl, "direction", (float[]){1.0f, 0.0f});

        gfx_draw_fullscreen_quad();

        fbo_unbind();

        fbo_bind(&bloombfb);

        program_use(&bloombl);

        fbo_bind_texture(&bloomblfb, 0, 0);

        program_set_int(&bloombl, "image", 0);
        program_set_vec2(&bloombl, "direction", (float[]){0.0f, 1.0f});

        gfx_draw_fullscreen_quad();

        fbo_unbind();

        program_use(&pp);

        fbo_bind_texture(&ppfb, 0, 0);
        fbo_bind_texture(&gbuffer, 2, 1);
        fbo_bind_texture(&bloombfb, 0, 2);

        program_set_int(&pp, "colorTexture", 0);
        program_set_int(&pp, "velocityTexture", 1);
        program_set_int(&pp, "bloomTexture", 2);

        gfx_draw_fullscreen_quad();

        game_draw_hud();

        glfwSwapBuffers(_win->glwin);
    }

    game_destroy();

    glfwTerminate();

    return 0;
}