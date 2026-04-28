#include "log.h"
#include "win.h"
#include "glad.h"
#include "shader.h"
#include "fm.h"
#include "cam.h"
#include "tex.h"
#include "tile.h"
#include "gfx.h"
#include "game.h"
#include "input.h"
#include <GLFW/glfw3.h>
#include <cglm/cglm.h>

int main() {
    ASSERT(glfwInit(), "no glfw");

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GL_DEPTH_BITS, 24);

    Window packet;

    packet.width = 800;
    packet.height = 600;
    packet.title = "blocks";

    window_create(&packet);

    ASSERT(gladLoadGLLoader((GLADloadproc)glfwGetProcAddress), "no glad");

    glfwSwapInterval(0);

    glfwSetInputMode(packet.glwin, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
    glfwSetInputMode(packet.glwin, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glClearColor(0.1f, 0.2f, 0.2f, 1.0f);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    game_init();
    double last_time;
    float fps_timer = 0.0f;
    int fps_counter = 0;

    while (!window_shouldclose()) {
        double current_time = glfwGetTime();
        float delta = (float)(current_time - last_time);
        last_time = current_time;

        fps_timer += delta;
        fps_counter++;

        if (fps_timer >= 1.0f) {
            printf("FPS: %d\n", fps_counter);
            fps_counter = 0;
            fps_timer = 0.0f;
        }

        window_update();
        window_draw();
        game_tick();
        game_draw();
    }
}