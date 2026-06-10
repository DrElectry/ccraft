#include "utils/log.h"
#include "core/win.h"
#include "glad.h"
#include "core/game.h"
#include "core/gfx.h"
#include "gl/fbo.h"
#include "utils/fm.h"
#include "gl/tex.h"
#include <GLFW/glfw3.h>
#include <cglm/cglm.h>
#include <stdio.h>
#include <string.h>
#include "core/main.h"
#include <unistd.h>
#include "network/network.h"
#include "utils/rand.h"
#include "network/net_platform.h"
#include <time.h>

uint64_t __servseed;

char __servip[256] = "127.0.0.1";
int __onserv;
int __servport;
char __nickname[32];

int main(int argc, char* argv[]) {
    if (net_init() != 0) {
        fprintf(stderr, "Failed to initialize Winsock\n");
        return 1;
    }
    rng_seed((unsigned int)time(NULL));
    char nnickname[32];
    snprintf(nnickname, sizeof(nnickname), "player%i", RAND(0, 9999));
    strcpy(__nickname, nnickname);
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-connect") == 0)
        {
            if (i + 1 < argc)
            {
                char* address = argv[i + 1];

                char* colon = strchr(address, ':');

                if (colon)
                {
                    size_t ip_len = colon - address;
                    
                    if (ip_len >= sizeof(__servip)) {
                        fprintf(stderr, "IP too long\n");
                        return 1;
                    }
                    
                    strncpy(__servip, address, ip_len);
                    __servip[ip_len] = '\0';
                    
                    if (strcmp(__servip, "localhost") == 0) {
                        strcpy(__servip, "127.0.0.1");
                    }
                    
                    __servport = atoi(colon + 1);
                    __onserv = 1;
                }
                else
                {
                    fprintf(stderr, "invalid format. use ip:port\n");
                    return 1;
                }
            }
        }
        if (strcmp(argv[i], "-nickname") == 0) {
            if (i + 1 < argc) {
                char* nickname = argv[i + 1];
                if (strlen(nickname) > 32) {
                    snprintf(nnickname, sizeof(nnickname), "player%i", RAND(0, 9999));
                    printf("Nickname too long! Using: %s\n", nnickname);
                    strcpy(__nickname, nnickname);
                } else {
                    strcpy(__nickname, nickname);
                }
            }
        }
    }

    printf("Playing as <%s>\n", __nickname);

    if (__onserv)
    {
        char connect_ip[256];
        strcpy(connect_ip, __servip);
        printf("Connecting to %s:%d\n", connect_ip, __servport);
        
        if (network_connect_and_handshake(__servip, __servport, &__servseed, __nickname) != 0)
        {
            fprintf(stderr, "network handshake failed\n");
            return 1;
        }
        
        printf("Connected.\n");
        printf("\n%s\n", network_get_server_name());
        
        const char* desc = network_get_server_description();
        if (desc && desc[0] != '\0') {
            printf("%s\n", desc);
        }
    }

    ASSERT(glfwInit(), "no glfw");

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_DEPTH_BITS, 24);

    Window packet;

    packet.width = WIDTH;
    packet.height = HEIGHT;
    packet.title = "ccraft";

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
    FBO ssaoblurfb;
    FBO ppfb;
    FBO prev_frame;
    FBO ssrfb;
    FBO bloomfb;
    FBO blurfb;
    FBO ffb;

    Shader mainv, mainf, ssaof, ppf, ssrf, bloomf, blurf, crossf;
    Program main, ssao, pp, ssr, blur, bloom, cross;
    File mainfv, mainff, ssaoff, ppff, ssrff, bloomff, blurff, crossff;

    Texture crosshair;

    crosshair.mag_filter = GL_NEAREST;
    crosshair.min_filter = GL_NEAREST;
    crosshair.wrap_s = GL_REPEAT;
    crosshair.wrap_t = GL_REPEAT;

    texture_create(&crosshair, "assets/textures/crosshair.png");

    mainv.type = GL_VERTEX_SHADER;
    mainf.type = GL_FRAGMENT_SHADER;
    ssaof.type = GL_FRAGMENT_SHADER;
    ppf.type = GL_FRAGMENT_SHADER;
    ssrf.type = GL_FRAGMENT_SHADER;
    bloomf.type = GL_FRAGMENT_SHADER;
    blurf.type = GL_FRAGMENT_SHADER;
    crossf.type = GL_FRAGMENT_SHADER;

    mainfv = file_open("assets/deferred/main.vsh");
    mainff = file_open("assets/deferred/main.fsh");
    ssaoff = file_open("assets/deferred/ssao.fsh");
    ppff = file_open("assets/deferred/pp.fsh");
    ssrff = file_open("assets/deferred/ssr.fsh");
    bloomff = file_open("assets/deferred/bloom.fsh");
    blurff = file_open("assets/deferred/blur.fsh");
    crossff = file_open("assets/gui/crosshair.fsh");

    shader_create(&mainv, mainfv.data);
    shader_create(&mainf, mainff.data);
    shader_create(&ssaof, ssaoff.data);
    shader_create(&ppf, ppff.data);
    shader_create(&ssrf, ssrff.data);
    shader_create(&bloomf, bloomff.data);
    shader_create(&blurf, blurff.data);
    shader_create(&crossf, crossff.data);

    program_create(&main, &mainv, &mainf);
    program_create(&ssao, &mainv, &ssaof);
    program_create(&pp, &mainv, &ppf);
    program_create(&ssr, &mainv, &ssrf);
    program_create(&blur, &mainv, &blurf);
    program_create(&bloom, &mainv, &bloomf);
    program_create(&cross, &mainv, &crossf);

    ssrfb.color_formats[0] = FBO_COLOR_RGBA16F;
    bloomfb.color_formats[0] = FBO_COLOR_RGBA16F;
    blurfb.color_formats[0] = FBO_COLOR_RGBA16F;
    ssaoblurfb.color_formats[0] = FBO_COLOR_RGBA16F;
    ffb.color_formats[1] = FBO_COLOR_RGBA16F;

    fbo_create(&ssaofb, WIDTH/2, HEIGHT/2, 1);
    fbo_create(&ssaoblurfb, WIDTH/2, HEIGHT/2, 1);
    fbo_create(&prev_frame, WIDTH, HEIGHT, 1);
    fbo_create(&ppfb, WIDTH, HEIGHT, 1);
    fbo_create(&ssrfb, WIDTH/2, HEIGHT/2, 1);
    fbo_create(&bloomfb, WIDTH/2, HEIGHT/2, 1);
    fbo_create(&blurfb, WIDTH/2, HEIGHT/2, 1);
    fbo_create(&ffb, WIDTH, HEIGHT, 1);

    gbuffer.color_formats[0] = FBO_COLOR_RGB16F;
    gbuffer.color_formats[1] = FBO_COLOR_RGB16F;
    gbuffer.color_formats[2] = FBO_COLOR_RGBA16F;
    gbuffer.color_formats[3] = FBO_COLOR_RG16F;

    fbo_create(&gbuffer, WIDTH, HEIGHT, 4);

    fbo_create_depth(&shadow_pass, 2048, 2048);

    float ssao_ratio[2] = {(float)WIDTH / (WIDTH/2), (float)HEIGHT / (HEIGHT/2)};
    float ssr_ratio[2] = {(float)WIDTH / (WIDTH/2), (float)HEIGHT / (HEIGHT/2)};
    float bloom_ratio[2] = {(float)WIDTH / (WIDTH/2), (float)HEIGHT / (HEIGHT/2)};

    game_init();

    double last_time = glfwGetTime();
    float fps_timer = 0.0f;
    int fps_counter = 0;
    float time = 0.0f;

    while (!window_shouldclose()) {
        double current_time = glfwGetTime();
        float delta_time = (float)(current_time - last_time);
        last_time = current_time;

        time += delta_time;

        game_tick(delta_time);

        if (wireframe==1 || potato_mode==1) {
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

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);

        // yuck yuck copying
        fbo_bind(&prev_frame);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, prev_frame.id);
        glBlitFramebuffer(0, 0, WIDTH, HEIGHT, 0, 0, WIDTH, HEIGHT, GL_COLOR_BUFFER_BIT, GL_LINEAR);
        fbo_unbind();

        fbo_bind(&gbuffer);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

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

        program_set_vec2(&ssao, "ssao_ratio", ssao_ratio);

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);

        gfx_draw_fullscreen_quad();

        fbo_unbind();

        fbo_bind(&ssaoblurfb);

        program_use(&blur);

        fbo_bind_texture(&ssaofb, 0, 0);

        program_set_int(&blur, "image", 0);
        program_set_vec2(&blur, "direction", (float[]){1.0f, 0.0f});
        program_set_vec2(&blur, "blur_ratio", ssao_ratio);
        program_set_float(&blur, "blurScale", 1.0f);

        gfx_draw_fullscreen_quad();

        fbo_unbind();

        fbo_bind(&ssaofb);

        program_use(&blur);

        fbo_bind_texture(&ssaoblurfb, 0, 0);

        program_set_int(&blur, "image", 0);
        program_set_vec2(&blur, "direction", (float[]){0.0f, 1.0f});
        program_set_float(&blur, "blurScale", 1.0f);
        program_set_vec2(&blur, "blur_ratio", ssao_ratio);

        gfx_draw_fullscreen_quad();

        fbo_unbind();

        fbo_bind(&ssrfb);

        program_use(&ssr);

        fbo_bind_texture(&prev_frame, 0, 0);
        fbo_bind_texture(&gbuffer, 1, 1);
        fbo_bind_texture(&gbuffer, 2, 3);
        fbo_bind_texture(&gbuffer, 3, 4);
        fbo_bind_depth_texture(&gbuffer, 5);

        program_set_int(&ssr, "gAlbedo", 0);
        program_set_int(&ssr, "gNormal", 1);
        program_set_int(&ssr, "gPosition", 3);
        program_set_int(&ssr, "gRoughness", 4);
        program_set_int(&ssr, "gDepth", 5);

        program_set_mat4(&ssr, "projection", (float*)projection);
        program_set_mat4(&ssr, "view", (float*)view);

        program_set_vec2(&ssr, "ssr_ratio", ssr_ratio);

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
        program_set_vec3(&main, "lightColor", (float[]){1.0f, 1.0f, 1.0f});

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);

        gfx_draw_fullscreen_quad();

        fbo_unbind();

        fbo_bind(&bloomfb);

        program_use(&bloom);

        fbo_bind_texture(&ppfb, 0, 0);

        program_set_int(&bloom, "image", 0);
        program_set_vec2(&bloom, "bloom_ratio", bloom_ratio);

        gfx_draw_fullscreen_quad();

        fbo_unbind();

        fbo_bind(&blurfb);

        program_use(&blur);

        fbo_bind_texture(&bloomfb, 0, 0);

        program_set_int(&blur, "image", 0);
        program_set_vec2(&blur, "direction", (float[]){1.0f, 0.0f});
        program_set_float(&blur, "blurScale", 2.0f);
        program_set_vec2(&blur, "blur_ratio", bloom_ratio);

        gfx_draw_fullscreen_quad();

        fbo_unbind();

        fbo_bind(&bloomfb);

        program_use(&blur);

        fbo_bind_texture(&blurfb, 0, 0);

        program_set_int(&blur, "image", 0);
        program_set_vec2(&blur, "direction", (float[]){0.0f, 1.0f});
        program_set_float(&blur, "blurScale", 2.0f);
        program_set_vec2(&blur, "blur_ratio", bloom_ratio);

        gfx_draw_fullscreen_quad();

        fbo_unbind();

        fbo_bind(&ffb);

        program_use(&pp);

        fbo_bind_texture(&ppfb, 0, 0);
        fbo_bind_depth_texture(&gbuffer, 1);
        fbo_bind_texture(&bloomfb, 0, 2);

        program_set_int(&pp, "colorTexture", 0);
        program_set_int(&pp, "depthTexture", 1);
        program_set_int(&pp, "bloomTexture", 2);

        gfx_draw_fullscreen_quad();

        fbo_unbind();

        program_use(&cross);

        fbo_bind_texture(&ffb, 0, 0);
        texture_bind(&crosshair, 1);

        program_set_int(&cross, "crosshair", 1);
        program_set_float(&cross, "width", (float)WIDTH);
        program_set_float(&cross, "height", (float)HEIGHT);

        gfx_draw_fullscreen_quad();

        game_draw_hud();

        glfwSwapBuffers(_win->glwin);
    }

    game_destroy();

    glfwTerminate();

    return 0;
}