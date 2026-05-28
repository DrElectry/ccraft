#include "core/input.h"

static int prev_keys[512];
static int curr_keys[512];

static Input* g_input = NULL;

static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    if (g_input) {
        g_input->scroll_x += xoffset;
        g_input->scroll_y += yoffset;
    }
}

void input_init(Input* in, GLFWwindow* win) {
    in->win = win;

    in->mouse_x = 0.0;
    in->mouse_y = 0.0;

    in->last_mouse_x = 0.0;
    in->last_mouse_y = 0.0;

    in->mouse_dx = 0.0;
    in->mouse_dy = 0.0;

    in->scroll_x = 0.0;
    in->scroll_y = 0.0;

    in->first_mouse = 1;

    for (int i = 0; i < 512; i++) {
        prev_keys[i] = GLFW_RELEASE;
        curr_keys[i] = GLFW_RELEASE;
    }

    g_input = in;

    glfwSetScrollCallback(win, scroll_callback);
}

void input_update(Input* in) {

    for (int i = 0; i < 512; i++) {
        prev_keys[i] = curr_keys[i];
        curr_keys[i] = glfwGetKey(in->win, i);
    }

    glfwGetCursorPos(in->win, &in->mouse_x, &in->mouse_y);

    if (in->first_mouse) {
        in->last_mouse_x = in->mouse_x;
        in->last_mouse_y = in->mouse_y;
        in->first_mouse = 0;
    }

    in->mouse_dx = in->mouse_x - in->last_mouse_x;
    in->mouse_dy = in->mouse_y - in->last_mouse_y;

    in->last_mouse_x = in->mouse_x;
    in->last_mouse_y = in->mouse_y;
}

int input_down(Input* in, int key) {
    return curr_keys[key] == GLFW_PRESS;
}

int input_pressed(Input* in, int key) {
    return curr_keys[key] == GLFW_PRESS &&
           prev_keys[key] == GLFW_RELEASE;
}

int input_released(Input* in, int key) {
    return curr_keys[key] == GLFW_RELEASE &&
           prev_keys[key] == GLFW_PRESS;
}