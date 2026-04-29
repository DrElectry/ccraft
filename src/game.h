#ifndef GAME_H
#define GAME_H

#include "text.h"
#include <cglm/cglm.h>

extern mat4 projection, view, inv_projection, inv_view, light_proj, light_view, light_space_matrix;

void game_init();
void game_draw();
void game_tick(float dt);
void game_shadow_pass(void);
void game_destroy();

#endif
