#ifndef SKIN_H
#define SKIN_H

#include "glad.h"
#include "gl/shader.h"
#include "gl/vao.h"
#include "gl/vbo.h"
#include "gl/ebo.h"
#include <cglm/cglm.h>

typedef struct {
    vec3 position;
    vec4 rotation;
    vec3 scale;
} TransformKey;

typedef struct {
    float time;
    vec3 value;
} Vec3Key;

typedef struct {
    float time;
    vec4 value;
} QuatKey;

typedef struct {
    char bone_name[64];
    int bone_index;
    Vec3Key* pos_keys;
    int pos_count;
    QuatKey* rot_keys;
    int rot_count;
    Vec3Key* scl_keys;
    int scl_count;
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
    int* parent_indices;
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

#define SKIN_MAX_BONES 64

typedef struct {
    VBO vbo;
    VAO vao;
    EBO ebo;
    unsigned int bone_ubo;
    GLenum index_type;
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
    mat4 node_transform;

    SkinBuffer gpu;
    char anim_name[64];
} Skinned;

typedef struct {
    int enabled;
    float head_yaw;
    float head_pitch;
    int head_bone;
    int neck_bone;
} SkinnedLook;

Skeleton* skeleton_create(int bone_count);
void skeleton_destroy(Skeleton* s);
void skeleton_set_bone_name(Skeleton* s, int index, const char* name);
void skeleton_set_inverse_bind_pose(Skeleton* s, int index, mat4 pose);
void skeleton_set_bone_offset(Skeleton* s, int index, mat4 offset);
void skeleton_set_parent(Skeleton* s, int index, int parent_index);
void skeleton_calc_global_matrices(Skeleton* s, mat4* locals, mat4* globals);
int skeleton_find_bone(Skeleton* s, const char* name);

float skinned_angle_diff(float from, float to);
float skinned_update_body_yaw(float body_yaw, float look_yaw, float dt);
float skinned_head_yaw_offset(float body_yaw, float look_yaw);

AnimationClip* animation_clip_create(const char* name);
void animation_clip_destroy(AnimationClip* clip);
void animation_clip_add_track(AnimationClip* clip, BoneTrack track);
void animation_track_add_pos_key(BoneTrack* track, float time, vec3 value);
void animation_track_add_rot_key(BoneTrack* track, float time, vec4 value);
void animation_track_add_scl_key(BoneTrack* track, float time, vec3 value);

AnimState* anim_state_create(AnimationClip* clip);
void anim_state_destroy(AnimState* state);
void anim_state_play(AnimState* state);
void anim_state_stop(AnimState* state);
void anim_state_pause(AnimState* state);
void anim_state_update(AnimState* state, float delta_time);

void anim_calc_bone_matrices(AnimState* state, Skeleton* skeleton, mat4* out_matrices);
void anim_get_final_matrices(Skeleton* skeleton, mat4* bone_transforms, mat4* out_matrices);

void skinned_cache(Skinned* s);
void skinned_render(Skinned* s, Program* active_program, float delta_time, const SkinnedLook* look);
void skinned_set_animation(Skinned* s, const char* name);
void skinned_play(Skinned* s);
void skinned_stop(Skinned* s);
void skinned_destroy(Skinned* s);
void skinned_program_init(Program* program);

#endif