#ifndef TILE_H
#define TILE_H
#include <cglm/cglm.h>

typedef struct {
    int* face_textures;
} Tile;

enum Tile_face {
    FRONT,
    BACK,
    LEFT,
    RIGHT,
    UP,
    DOWN
};

extern const float face_vertices[];
extern const unsigned int face_indices[];

float* tile_gen_uv(int atlas_number);

#endif