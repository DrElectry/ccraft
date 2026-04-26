#include "input.h"

static int prev_keys[512];
static int curr_keys[512];

void input_init(Input* in, GLFWwindow* win) {
    in->win = win;
}

void input_update(Input* in) {
    for (int i = 0; i < 512; i++) {
        prev_keys[i] = curr_keys[i];
        curr_keys[i] = glfwGetKey(in->win, i);
    }
}

int input_down(Input* in, int key) {
    return curr_keys[key] == GLFW_PRESS;
}

int input_pressed(Input* in, int key) {
    return curr_keys[key] == GLFW_PRESS && prev_keys[key] == GLFW_RELEASE;
}