#include "core/input.h"

static int prev_keys[512];
static int curr_keys[512];

static Input* g_input = NULL;

static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    (void)window;
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
    in->chat_active = 0;

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

    if (in->chat_active) {
        in->mouse_dx = 0.0;
        in->mouse_dy = 0.0;
        in->scroll_x = 0.0;
        in->scroll_y = 0.0;
        glfwGetCursorPos(in->win, &in->mouse_x, &in->mouse_y);
        in->last_mouse_x = in->mouse_x;
        in->last_mouse_y = in->mouse_y;
        return;
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

int input_chat_active(const Input* in) {
    return in && in->chat_active;
}

void input_set_chat_active(Input* in, int active) {
    if (!in) return;

    in->chat_active = active ? 1 : 0;

    if (active) {
        glfwSetInputMode(in->win, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        in->mouse_dx = 0.0;
        in->mouse_dy = 0.0;
        in->scroll_x = 0.0;
        in->scroll_y = 0.0;
    } else {
        glfwSetInputMode(in->win, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        in->first_mouse = 1;
    }
}

int input_key_to_char(int key, int shift) {
    if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) {
        return 'A' + (key - GLFW_KEY_A);
    }

    if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) {
        if (!shift) {
            return '0' + (key - GLFW_KEY_0);
        }
        switch (key) {
            case GLFW_KEY_1: return '!';
            case GLFW_KEY_0: return ')';
            case GLFW_KEY_7: return '&';
            default: return 0;
        }
    }

    switch (key) {
        case GLFW_KEY_SPACE: return ' ';
        case GLFW_KEY_PERIOD: return '.';
        case GLFW_KEY_COMMA: return shift ? '<' : 0;
        case GLFW_KEY_SLASH: return shift ? '?' : '/';
        case GLFW_KEY_SEMICOLON: return shift ? ':' : 0;
        case GLFW_KEY_EQUAL: return shift ? '+' : '=';
        case GLFW_KEY_LEFT_BRACKET: return '(';
        case GLFW_KEY_RIGHT_BRACKET: return ')';
        default: return 0;
    }
}

void input_chat_poll_typed(Input* in, void (*on_char)(char c, void* user), void* user) {
    if (!in || !in->chat_active || !on_char) return;

    int shift = input_down(in, GLFW_KEY_LEFT_SHIFT) || input_down(in, GLFW_KEY_RIGHT_SHIFT);

    for (int key = GLFW_KEY_SPACE; key <= GLFW_KEY_LAST; key++) {
        if (!input_pressed(in, key)) continue;

        if (key == GLFW_KEY_ESCAPE || key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER ||
            key == GLFW_KEY_BACKSPACE) {
            continue;
        }


        char c = (char)input_key_to_char(key, shift);
        if (c) {
            on_char(c, user);
        }
    }
}
