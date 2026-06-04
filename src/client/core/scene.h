#ifndef SCENE_H
#define SCENE_H

#include <cglm/cglm.h>

void scene_init(void);
void scene_tick(float dt);
void scene_render_main(float time);
void scene_render_shadow(float dt);
void scene_render_hud(void);
void scene_destroy(void);

#endif

