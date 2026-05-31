#ifndef GLTF_H
#define GLTF_H

#include "core/skin.h"

typedef struct {
    Skinned* skinned;
    Skeleton* skeleton;
    AnimationClip** animations;
    int animation_count;
    char** animation_names;
} GLTFModel;

int gltf_load(const char* path, GLTFModel* out);
AnimationClip* gltf_get_animation(GLTFModel* model, const char* name);
void gltf_free(GLTFModel* model);

#endif