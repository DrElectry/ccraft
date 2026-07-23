#ifndef TILE_H
#define TILE_H

#include <stdint.h>

#include "core/gfx.h"
#include "gl/fbo.h"
#include "gl/shader.h"
#include "gl/tex.h"

#define TILE_ICON_SIZE 64

extern const float face_vertices[];
extern const unsigned int face_indices[];

enum Tile_face {
    FRONT,
    BACK,
    LEFT,
    RIGHT,
    UP,
    DOWN
};

extern Render_request tile_render_cache[19];
extern FBO tile_fbos[19];
extern Canvas_Render_Request tile_icons[19];

void tile_atlas_getuv(int atlas_number, float* uv);
void tile_push_face(float* vertices,
                    unsigned int* indices,
                    float* pos,
                    int* v_cursor,
                    int* i_cursor,
                    int face,
                    int atlas_id,
                    float light);
                    
void tile_push_cube(float* vertices, unsigned int* indices, float* pos, int* v_cursor, int* i_cursor);
void tile_create_cube_from_tile(uint16_t block_id, Render_request* out);
void tile_pre_render_all(Program* prog, Texture* atlas, Texture* roug_tex, Texture* norm_tex);

#endif
