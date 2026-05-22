#ifndef TILE_H
#define TILE_H

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

void tile_atlas_getuv(int atlas_number, float* uv);
void tile_push_face(float* vertices,
                    unsigned int* indices,
                    float* pos,          // vec3 (x, y, z)
                    int* v_cursor,
                    int* i_cursor,
                    int face,
                    int atlas_id);

#endif