#ifndef WIN_H
#define WIN_H

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <stdint.h>

typedef struct {
    uint16_t width;
    uint16_t height;

    char* title;
    GLFWwindow* glwin;
} Window;

void window_create(Window* packet);
int window_shouldclose();
void window_update();
void window_draw();

extern Window* _win;

#endif