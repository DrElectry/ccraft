#include "core/win.h"
#include "utils/log.h"
#include "glad.h"
#include <GLFW/glfw3.h>

typedef void (*ResizeCallback)(int width, int height);

Window* _win;
ResizeCallback _resize_callback = NULL;

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    if (_resize_callback)
    {
        _resize_callback(width, height);
    }
}

void window_create(Window* packet) {
    packet->glwin = glfwCreateWindow(packet->width, packet->height, packet->title, NULL, NULL);
    ASSERT(packet->glwin, "no window");
    glfwMakeContextCurrent(packet->glwin);

    _win = packet;
}

void window_hook_resize_call(ResizeCallback callback)
{
    _resize_callback = callback;
    glfwSetFramebufferSizeCallback(_win->glwin, framebuffer_size_callback);
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