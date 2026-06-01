#include "gui/chat.h"
#include "gui/text.h"
#include "core/main.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    char text[CHAT_LINE_MAX];
    float age;
    uint16_t color;
    HText hud;
    int active;
} ChatLine;

static ChatLine chat_lines[CHAT_MAX_LINES];
static int chat_line_count = 0;

static char chat_compose[CHAT_COMPOSE_MAX];
static HText chat_compose_hud;
static int chat_compose_hud_active = 0;
static int chat_typing = 0;

static void chat_sanitize_char(char* dst, size_t dst_len, char c) {
    if (!dst || dst_len == 0) return;

    size_t len = strlen(dst);
    if (len + 1 >= dst_len) return;

    unsigned char uc = (unsigned char)c;
    if (uc >= 'a' && uc <= 'z') {
        uc = (unsigned char)(uc - 'a' + 'A');
    }
    dst[len] = (char)uc;
    dst[len + 1] = '\0';
}

static void chat_sanitize(char* dst, const char* src, size_t dst_size) {
    if (!dst || dst_size == 0) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }

    dst[0] = '\0';
    for (size_t i = 0; src[i] != '\0' && strlen(dst) + 1 < dst_size; i++) {
        chat_sanitize_char(dst, dst_size, src[i]);
    }
}

static void chat_remove_at(int index) {
    if (index < 0 || index >= chat_line_count) return;

    if (chat_lines[index].active) {
        text_free(&chat_lines[index].hud);
    }
    chat_lines[index].active = 0;

    if (index < chat_line_count - 1) {
        memmove(&chat_lines[index], &chat_lines[index + 1],
                (size_t)(chat_line_count - index - 1) * sizeof(ChatLine));
    }
    chat_line_count--;
}

static void chat_expire_old(void) {
    for (int i = chat_line_count - 1; i >= 0; i--) {
        if (chat_lines[i].age >= CHAT_LIFETIME_SEC) {
            chat_remove_at(i);
        }
    }
}

static int chat_compose_screen_y(void) {
    int y = (int)HEIGHT - CHAT_MARGIN_Y - CHAR_HEIGHT;
    if (chat_line_count > 0) {
        y -= chat_line_count * (CHAR_HEIGHT + CHAT_LINE_GAP);
    }
    return y;
}

static void chat_sync_compose_hud(void) {
    static char display[CHAT_COMPOSE_MAX + 4];

    if (!chat_typing) {
        if (chat_compose_hud_active) {
            text_free(&chat_compose_hud);
            chat_compose_hud_active = 0;
        }
        return;
    }

    snprintf(display, sizeof(display), "> %s", chat_compose);
    text_create(&chat_compose_hud, display, 0x0Fu, CHAT_MARGIN_X, chat_compose_screen_y());
    chat_compose_hud_active = 1;
}

static void chat_layout_and_sync(void) {
    int y = (int)HEIGHT - CHAT_MARGIN_Y - CHAR_HEIGHT;

    for (int i = chat_line_count - 1; i >= 0; i--) {
        if (!chat_lines[i].active) continue;

        chat_lines[i].hud.x = CHAT_MARGIN_X;
        chat_lines[i].hud.y = y;
        text_create(&chat_lines[i].hud, chat_lines[i].text, chat_lines[i].color,
                    chat_lines[i].hud.x, chat_lines[i].hud.y);

        y -= CHAR_HEIGHT + CHAT_LINE_GAP;
    }

    chat_sync_compose_hud();
}

static void chat_begin(Input* in) {
    chat_compose[0] = '\0';
    chat_typing = 1;
    input_set_chat_active(in, 1);
    chat_sync_compose_hud();
}

static void chat_cancel(Input* in) {
    chat_compose[0] = '\0';
    chat_typing = 0;
    input_set_chat_active(in, 0);
    chat_sync_compose_hud();
}

static void chat_submit(Input* in) {
    if (chat_compose[0] != '\0') {
        chat_push(__nickname, chat_compose);
    }
    chat_cancel(in);
}

static void chat_append_char(char c, void* user) {
    (void)user;
    chat_sanitize_char(chat_compose, sizeof(chat_compose), c);
    chat_sync_compose_hud();
}

void chat_init(void) {
    memset(chat_lines, 0, sizeof(chat_lines));
    chat_line_count = 0;
    chat_compose[0] = '\0';
    chat_compose_hud_active = 0;
}

void chat_destroy(void) {
    if (chat_compose_hud_active) {
        text_free(&chat_compose_hud);
        chat_compose_hud_active = 0;
    }

    for (int i = 0; i < chat_line_count; i++) {
        if (chat_lines[i].active) {
            text_free(&chat_lines[i].hud);
        }
    }
    chat_line_count = 0;
}

void chat_update(float dt) {
    if (chat_line_count == 0 && !chat_is_typing()) return;

    for (int i = 0; i < chat_line_count; i++) {
        chat_lines[i].age += dt;
    }

    chat_expire_old();
    chat_layout_and_sync();
}

void chat_draw(void) {
    for (int i = 0; i < chat_line_count; i++) {
        if (!chat_lines[i].active) continue;
        text_draw(&chat_lines[i].hud);
    }

    if (chat_compose_hud_active) {
        text_draw(&chat_compose_hud);
    }
}

void chat_push_line(const char* line, uint16_t color) {
    if (!line || line[0] == '\0') return;

    if (chat_line_count >= CHAT_MAX_LINES) {
        chat_remove_at(0);
    }

    ChatLine* slot = &chat_lines[chat_line_count++];
    memset(slot, 0, sizeof(ChatLine));

    chat_sanitize(slot->text, line, sizeof(slot->text));
    if (slot->text[0] == '\0') {
        chat_line_count--;
        return;
    }

    slot->age = 0.0f;
    slot->color = color ? color : CHAT_DEFAULT_COLOR;
    slot->active = 1;

    int y = (int)HEIGHT - CHAT_MARGIN_Y - CHAR_HEIGHT;
    text_create(&slot->hud, slot->text, slot->color, CHAT_MARGIN_X, y);
}

void chat_push(const char* nickname, const char* message) {
    char line[CHAT_LINE_MAX];

    if (!nickname) nickname = "";
    if (!message) message = "";

    if (nickname[0] != '\0') {
        snprintf(line, sizeof(line), "%s: %s", nickname, message);
    } else {
        snprintf(line, sizeof(line), "%s", message);
    }

    chat_push_line(line, CHAT_DEFAULT_COLOR);
}

int chat_is_typing(void) {
    return chat_typing;
}

void chat_handle_input(Input* in) {
    if (!in) return;

    if (!chat_typing) {
        if (__onserv && input_pressed(in, GLFW_KEY_T)) {
            chat_begin(in);
        }
        return;
    }

    if (input_pressed(in, GLFW_KEY_ESCAPE)) {
        chat_cancel(in);
        return;
    }

    if (input_pressed(in, GLFW_KEY_ENTER) || input_pressed(in, GLFW_KEY_KP_ENTER)) {
        chat_submit(in);
        return;
    }

    if (input_pressed(in, GLFW_KEY_BACKSPACE)) {
        size_t len = strlen(chat_compose);
        if (len > 0) {
            chat_compose[len - 1] = '\0';
            chat_sync_compose_hud();
        }
        return;
    }

    input_chat_poll_typed(in, chat_append_char, NULL);
}