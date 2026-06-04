#ifndef GLTF_H
#define GLTF_H

#include "core/skin.h"
#include "core/gfx.h"

typedef struct {
    Skinned* skinned;
    Skeleton* skeleton;
    AnimationClip** animations;
    int animation_count;
    char** animation_names;
    float feet_align_y;
} GLTFModel;

int gltf_load(const char* path, GLTFModel* out);
AnimationClip* gltf_get_animation(GLTFModel* model, const char* name);
void gltf_free(GLTFModel* model);
Skinned_render_request* gltf_load_skinned_request(const char* path);
void gltf_free_skinned_request(Skinned_render_request* request);

#endif