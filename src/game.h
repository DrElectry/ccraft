#ifndef GAME_H
#define GAME_H

#include "text.h"
#include "player.h"
#include <cglm/cglm.h>

extern mat4 projection, view, inv_projection, inv_view, light_proj, light_view, light_space_matrix;
extern mat4 prev_view_proj;
extern vec3 light_pos;
extern vec3 light_dir;
extern int wireframe;

extern Player player;

void game_init();
void game_draw(float time);
void game_tick(float dt);
void game_shadow_pass(void);
void game_draw_hud();
void game_destroy();

#endif
