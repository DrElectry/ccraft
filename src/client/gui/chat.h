#ifndef CHAT_H
#define CHAT_H

#include "core/input.h"
#include <stdint.h>

#define CHAT_COMPOSE_MAX 96

#define CHAT_MAX_LINES 10
#define CHAT_LINE_MAX 128
#define CHAT_LIFETIME_SEC 12.0f
#define CHAT_LINE_GAP 2
#define CHAT_MARGIN_X 4
#define CHAT_MARGIN_Y 4
#define CHAT_DEFAULT_COLOR 0x0Fu

void chat_init(void);
void chat_destroy(void);

void chat_update(float dt);
void chat_draw(void);

void chat_push(const char* nickname, const char* message);
void chat_push_line(const char* line, uint16_t color);

void chat_handle_input(Input* in);
int chat_is_typing(void);

#endif
