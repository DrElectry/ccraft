#ifndef INPUT_H
#define INPUT_H

#include <GLFW/glfw3.h>

typedef struct {
    GLFWwindow* win;

    double mouse_x;
    double mouse_y;

    double last_mouse_x;
    double last_mouse_y;

    double mouse_dx;
    double mouse_dy;

    int first_mouse;
} Input;

void input_init(Input* in, GLFWwindow* win);
void input_update(Input* in);

int input_down(Input* in, int key);
int input_pressed(Input* in, int key);

#endif