#include "log.h"
#include "win.h"
#include "glad.h"
#include <GLFW/glfw3.h>

int main() {
    ASSERT(glfwInit());

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    Window packet;

    packet.width = 800;
    packet.height = 600;
    packet.title = "blocks";

    window_create(&packet);

    ASSERT(gladLoadGLLoader((GLADloadproc)glfwGetProcAddress));

    glClearColor(0.0f, 1.0f, 1.0f, 1.0f);

    while (!window_shouldclose()) {
        window_update();
        window_draw();
    }
}