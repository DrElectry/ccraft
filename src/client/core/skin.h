#ifndef SKIN_H
#define SKIN_H

#include "gl/vao.h"
#include "gl/vbo.h"
#include "gl/ebo.h"
#include "gl/shader.h"
#include <cglm/cglm.h>

typedef struct {
    vec3 position;
    vec4 rotation;
    vec3 scale;
} TransformKey;

typedef struct {
    float time;
    TransformKey transform;
} Keyframe;

typedef struct {
    Keyframe* keyframes;
    int keyframe_count;
    char bone_name[64];
    int bone_index;
} BoneTrack;

typedef struct {
    BoneTrack* tracks;
    int track_count;
    float duration;
    float ticks_per_second;
    char name[64];
} AnimationClip;

typedef struct {
    mat4* inverse_bind_poses;
    mat4* bone_offsets;
    char** bone_names;
    int bone_count;
} Skeleton;

typedef struct {
    AnimationClip* clip;
    float current_time;
    float speed;
    int loop;
    int is_playing;
} AnimState;

typedef struct {
    VBO vbo;
    VAO vao;
    EBO ebo;
    Skeleton* skeleton;
    AnimState* anim;
} SkinBuffer;

typedef struct {
    float* vertices;
    int* indices;
    int vertex_count;
    int index_count;
    
    vec3 pos;
    vec3 rot;
    vec3 scale;
    
    SkinBuffer gpu;
    char anim_name[64];
} Skinned;

Skeleton* skeleton_create(int bone_count);
void skeleton_destroy(Skeleton* s);
void skeleton_set_bone_name(Skeleton* s, int index, const char* name);
void skeleton_set_inverse_bind_pose(Skeleton* s, int index, mat4 pose);
void skeleton_set_bone_offset(Skeleton* s, int index, mat4 offset);

AnimationClip* animation_clip_create(const char* name);
void animation_clip_destroy(AnimationClip* clip);
void animation_clip_add_track(AnimationClip* clip, BoneTrack track);
void animation_clip_add_keyframe(AnimationClip* clip, int track_index, float time, TransformKey transform);

AnimState* anim_state_create(AnimationClip* clip);
void anim_state_destroy(AnimState* state);
void anim_state_play(AnimState* state);
void anim_state_stop(AnimState* state);
void anim_state_pause(AnimState* state);
void anim_state_update(AnimState* state, float delta_time);

void anim_calc_bone_matrices(AnimState* state, Skeleton* skeleton, mat4* out_matrices);
void anim_get_final_matrices(Skeleton* skeleton, mat4* bone_transforms, mat4* out_matrices);

void skinned_cache(Skinned* s);
void skinned_render(Skinned* s, Program* active_program, float delta_time);
void skinned_set_animation(Skinned* s, const char* name);
void skinned_play(Skinned* s);
void skinned_stop(Skinned* s);
void skinned_destroy(Skinned* s);

#endif