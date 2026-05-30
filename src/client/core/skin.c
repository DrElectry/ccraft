#include "skin.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

static void quat_norm(vec4 q) {
    float mag = sqrtf(q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3]);
    if (mag > 0.0001f) {
        q[0] /= mag;
        q[1] /= mag;
        q[2] /= mag;
        q[3] /= mag;
    }
}

static void quat_slerp(vec4 q1, vec4 q2, float t, vec4 out) {
    float dot = q1[0]*q2[0] + q1[1]*q2[1] + q1[2]*q2[2] + q1[3]*q2[3];
    vec4 q2c = { q2[0], q2[1], q2[2], q2[3] };
    
    if (dot < 0.0f) {
        dot = -dot;
        q2c[0] = -q2c[0];
        q2c[1] = -q2c[1];
        q2c[2] = -q2c[2];
        q2c[3] = -q2c[3];
    }
    
    if (dot > 0.9995f) {
        out[0] = q1[0] + t * (q2c[0] - q1[0]);
        out[1] = q1[1] + t * (q2c[1] - q1[1]);
        out[2] = q1[2] + t * (q2c[2] - q1[2]);
        out[3] = q1[3] + t * (q2c[3] - q1[3]);
        quat_norm(out);
        return;
    }
    
    float theta_0 = acosf(dot);
    float theta = theta_0 * t;
    float sin_theta = sinf(theta);
    float sin_theta_0 = sinf(theta_0);
    float s1 = cosf(theta) - dot * sin_theta / sin_theta_0;
    float s2 = sin_theta / sin_theta_0;
    
    out[0] = q1[0] * s1 + q2c[0] * s2;
    out[1] = q1[1] * s1 + q2c[1] * s2;
    out[2] = q1[2] * s1 + q2c[2] * s2;
    out[3] = q1[3] * s1 + q2c[3] * s2;
    quat_norm(out);
}

static void vec3_lerp(vec3 v1, vec3 v2, float t, vec3 out) {
    out[0] = v1[0] + t * (v2[0] - v1[0]);
    out[1] = v1[1] + t * (v2[1] - v1[1]);
    out[2] = v1[2] + t * (v2[2] - v1[2]);
}

static void transform_to_mat(TransformKey* t, mat4 out) {
    mat4 trans, rot, scl;
    glm_translate_make(trans, t->position);
    glm_quat_mat4(t->rotation, rot);
    glm_scale_make(scl, t->scale);
    glm_mat4_mul(trans, rot, out);
    glm_mat4_mul(out, scl, out);
}

Skeleton* skeleton_create(int bone_count) {
    Skeleton* s = malloc(sizeof(Skeleton));
    s->bone_count = bone_count;
    s->inverse_bind_poses = malloc(sizeof(mat4) * bone_count);
    s->bone_offsets = malloc(sizeof(mat4) * bone_count);
    s->bone_names = malloc(sizeof(char*) * bone_count);
    
    for (int i = 0; i < bone_count; i++) {
        s->bone_names[i] = NULL;
        glm_mat4_identity(s->inverse_bind_poses[i]);
        glm_mat4_identity(s->bone_offsets[i]);
    }
    return s;
}

void skeleton_destroy(Skeleton* s) {
    if (!s) return;
    for (int i = 0; i < s->bone_count; i++) {
        if (s->bone_names[i]) free(s->bone_names[i]);
    }
    free(s->bone_names);
    free(s->inverse_bind_poses);
    free(s->bone_offsets);
    free(s);
}

void skeleton_set_bone_name(Skeleton* s, int index, const char* name) {
    if (index >= s->bone_count) return;
    if (s->bone_names[index]) free(s->bone_names[index]);
    s->bone_names[index] = malloc(strlen(name) + 1);
    strcpy(s->bone_names[index], name);
}

void skeleton_set_inverse_bind_pose(Skeleton* s, int index, mat4 pose) {
    if (index < s->bone_count) glm_mat4_copy(pose, s->inverse_bind_poses[index]);
}

void skeleton_set_bone_offset(Skeleton* s, int index, mat4 offset) {
    if (index < s->bone_count) glm_mat4_copy(offset, s->bone_offsets[index]);
}

AnimationClip* animation_clip_create(const char* name) {
    AnimationClip* clip = malloc(sizeof(AnimationClip));
    clip->tracks = NULL;
    clip->track_count = 0;
    clip->duration = 0.0f;
    clip->ticks_per_second = 30.0f;
    if (name) strcpy(clip->name, name);
    else clip->name[0] = '\0';
    return clip;
}

void animation_clip_destroy(AnimationClip* clip) {
    if (!clip) return;
    for (int i = 0; i < clip->track_count; i++) {
        if (clip->tracks[i].keyframes) free(clip->tracks[i].keyframes);
    }
    if (clip->tracks) free(clip->tracks);
    free(clip);
}

void animation_clip_add_track(AnimationClip* clip, BoneTrack track) {
    clip->tracks = realloc(clip->tracks, sizeof(BoneTrack) * (clip->track_count + 1));
    clip->tracks[clip->track_count] = track;
    clip->track_count++;
}

void animation_clip_add_keyframe(AnimationClip* clip, int track_index, float time, TransformKey transform) {
    if (track_index >= clip->track_count) return;
    
    BoneTrack* track = &clip->tracks[track_index];
    track->keyframes = realloc(track->keyframes, sizeof(Keyframe) * (track->keyframe_count + 1));
    
    int insert = track->keyframe_count;
    for (int i = 0; i < track->keyframe_count; i++) {
        if (track->keyframes[i].time > time) {
            insert = i;
            break;
        }
    }
    
    if (insert != track->keyframe_count) {
        for (int i = track->keyframe_count; i > insert; i--) {
            track->keyframes[i] = track->keyframes[i - 1];
        }
    }
    
    track->keyframes[insert].time = time;
    glm_vec3_copy(transform.position, track->keyframes[insert].transform.position);
    glm_vec4_copy(transform.rotation, track->keyframes[insert].transform.rotation);
    glm_vec3_copy(transform.scale, track->keyframes[insert].transform.scale);
    track->keyframe_count++;
    
    if (time > clip->duration) clip->duration = time;
}

AnimState* anim_state_create(AnimationClip* clip) {
    AnimState* state = malloc(sizeof(AnimState));
    state->clip = clip;
    state->current_time = 0.0f;
    state->speed = 1.0f;
    state->loop = true;
    state->is_playing = true;
    return state;
}

void anim_state_destroy(AnimState* state) {
    if (state) free(state);
}

void anim_state_play(AnimState* state) {
    state->is_playing = true;
}

void anim_state_stop(AnimState* state) {
    state->is_playing = false;
    state->current_time = 0.0f;
}

void anim_state_pause(AnimState* state) {
    state->is_playing = false;
}

void anim_state_update(AnimState* state, float delta_time) {
    if (!state->is_playing || !state->clip) return;
    
    state->current_time += delta_time * state->speed;
    
    if (state->current_time >= state->clip->duration) {
        if (state->loop) {
            state->current_time = fmodf(state->current_time, state->clip->duration);
        } else {
            state->current_time = state->clip->duration;
            state->is_playing = false;
        }
    }
}

static void get_interpolated(BoneTrack* track, float time, TransformKey* out) {
    if (track->keyframe_count == 0) {
        glm_vec3_zero(out->position);
        glm_quat_identity(out->rotation);
        glm_vec3_one(out->scale);
        return;
    }
    
    if (track->keyframe_count == 1 || time <= track->keyframes[0].time) {
        glm_vec3_copy(track->keyframes[0].transform.position, out->position);
        glm_vec4_copy(track->keyframes[0].transform.rotation, out->rotation);
        glm_vec3_copy(track->keyframes[0].transform.scale, out->scale);
        return;
    }
    
    if (time >= track->keyframes[track->keyframe_count - 1].time) {
        int last = track->keyframe_count - 1;
        glm_vec3_copy(track->keyframes[last].transform.position, out->position);
        glm_vec4_copy(track->keyframes[last].transform.rotation, out->rotation);
        glm_vec3_copy(track->keyframes[last].transform.scale, out->scale);
        return;
    }
    
    int next = 1;
    while (next < track->keyframe_count && track->keyframes[next].time < time) next++;
    int prev = next - 1;
    float t = (time - track->keyframes[prev].time) / (track->keyframes[next].time - track->keyframes[prev].time);
    
    vec3_lerp(track->keyframes[prev].transform.position, track->keyframes[next].transform.position, t, out->position);
    vec3_lerp(track->keyframes[prev].transform.scale, track->keyframes[next].transform.scale, t, out->scale);
    quat_slerp(track->keyframes[prev].transform.rotation, track->keyframes[next].transform.rotation, t, out->rotation);
}

void anim_calc_bone_matrices(AnimState* state, Skeleton* skeleton, mat4* out_matrices) {
    if (!state->clip || !skeleton) return;
    
    for (int i = 0; i < skeleton->bone_count; i++) {
        glm_mat4_copy(skeleton->bone_offsets[i], out_matrices[i]);
    }
    
    for (int i = 0; i < state->clip->track_count; i++) {
        BoneTrack* track = &state->clip->tracks[i];
        if (track->bone_index >= skeleton->bone_count) continue;
        
        TransformKey interp;
        get_interpolated(track, state->current_time, &interp);
        
        mat4 local;
        transform_to_mat(&interp, local);
        glm_mat4_copy(local, out_matrices[track->bone_index]);
    }
}

void anim_get_final_matrices(Skeleton* skeleton, mat4* bone_transforms, mat4* out_matrices) {
    for (int i = 0; i < skeleton->bone_count; i++) {
        glm_mat4_mul(bone_transforms[i], skeleton->inverse_bind_poses[i], out_matrices[i]);
    }
}

void skinned_cache(Skinned* s) {
    vao_create(&s->gpu.vao);
    vao_bind(&s->gpu.vao);
    
    vbo_create(&s->gpu.vbo, s->vertices, s->vertex_count * sizeof(float));
    ebo_create(&s->gpu.ebo, s->indices, s->index_count * sizeof(int));
    
    vbo_attr(0, 3, 16 * sizeof(float), 0);
    vbo_attr(1, 2, 16 * sizeof(float), 3);
    vbo_attr(2, 3, 16 * sizeof(float), 5);
    vbo_attr(3, 4, 16 * sizeof(float), 8);
    vbo_attr(4, 4, 16 * sizeof(float), 12);
    
    s->gpu.skeleton = NULL;
    s->gpu.anim = NULL;
    s->anim_name[0] = '\0';
}

void skinned_render(Skinned* s, Program* active_program, float delta_time) {
    if (!s->gpu.anim) return;
    
    mat4 model;
    glm_mat4_identity(model);
    glm_translate(model, s->pos);
    glm_rotate(model, s->rot[0], (vec3){1.0f, 0.0f, 0.0f});
    glm_rotate(model, s->rot[1], (vec3){0.0f, 1.0f, 0.0f});
    glm_rotate(model, s->rot[2], (vec3){0.0f, 0.0f, 1.0f});
    glm_scale(model, s->scale);
    
    anim_state_update(s->gpu.anim, delta_time);
    
    int bone_count = s->gpu.skeleton->bone_count;
    mat4* bone_mats = malloc(sizeof(mat4) * bone_count);
    mat4* final_mats = malloc(sizeof(mat4) * bone_count);
    
    anim_calc_bone_matrices(s->gpu.anim, s->gpu.skeleton, bone_mats);
    anim_get_final_matrices(s->gpu.skeleton, bone_mats, final_mats);
    
    program_use(active_program);
    program_set_mat4(active_program, "model", (float*)model);
    program_set_mat4_array(active_program, "bones", (float*)final_mats, bone_count);
    
    vao_bind(&s->gpu.vao);
    glDrawElements(GL_TRIANGLES, s->index_count, GL_UNSIGNED_INT, NULL);
    
    free(bone_mats);
    free(final_mats);
}

void skinned_set_animation(Skinned* s, const char* name) {
    strcpy(s->anim_name, name);
}

void skinned_play(Skinned* s) {
    if (s->gpu.anim) {
        anim_state_play(s->gpu.anim);
    }
}

void skinned_stop(Skinned* s) {
    if (s->gpu.anim) {
        anim_state_stop(s->gpu.anim);
    }
}

void skinned_destroy(Skinned* s) {
    if (s->gpu.anim) {
        anim_state_destroy(s->gpu.anim);
    }
    if (s->gpu.skeleton) {
        skeleton_destroy(s->gpu.skeleton);
    }
}