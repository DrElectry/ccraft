#include "win.h"
#include "log.h"
#include "glad.h"
#include <GLFW/glfw3.h>

Window* _win;

void window_create(Window* packet) {
    packet->glwin = glfwCreateWindow(packet->width, packet->height, packet->title, NULL, NULL);
    ASSERT(packet->glwin, "no window");
    glfwMakeContextCurrent(packet->glwin);

    _win = packet;
}

int window_shouldclose() {
    return glfwWindowShouldClose(_win->glwin);
}

void window_update() {
    glfwPollEvents();
}

void window_draw() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}