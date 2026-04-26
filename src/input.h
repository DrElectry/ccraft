#pragma once
#include <GLFW/glfw3.h>

typedef struct {
    GLFWwindow* win;
} Input;

void input_init(Input* in, GLFWwindow* win);
void input_update(Input* in);

int input_down(Input* in, int key);
int input_pressed(Input* in, int key);