#include "skin.h"
#include "glad.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

void skinned_program_init(Program* program) {
    if (!program) return;
    GLuint block = glGetUniformBlockIndex(program->id, "BoneData");
    if (block == GL_INVALID_INDEX) {
        printf("skinned_program_init: BoneData uniform block not found\n");
        return;
    }
    glUniformBlockBinding(program->id, block, 0);
}

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
    s->parent_indices = malloc(sizeof(int) * bone_count);
    s->bone_names = malloc(sizeof(char*) * bone_count);

    for (int i = 0; i < bone_count; i++) {
        s->bone_names[i] = NULL;
        s->parent_indices[i] = -1;
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
    free(s->parent_indices);
    free(s->inverse_bind_poses);
    free(s->bone_offsets);
    free(s);
}

void skeleton_set_parent(Skeleton* s, int index, int parent_index) {
    if (index < s->bone_count) s->parent_indices[index] = parent_index;
}

void skeleton_calc_global_matrices(Skeleton* s, mat4* locals, mat4* globals) {
    int* resolved = (int*)calloc((size_t)s->bone_count, sizeof(int));
    if (!resolved) return;

    for (int i = 0; i < s->bone_count; i++) {
        glm_mat4_copy(locals[i], globals[i]);
    }

    for (int pass = 0; pass < s->bone_count; pass++) {
        for (int i = 0; i < s->bone_count; i++) {
            if (resolved[i]) continue;

            int parent = s->parent_indices[i];
            if (parent >= 0 && !resolved[parent]) continue;

            if (parent < 0) {
                glm_mat4_copy(locals[i], globals[i]);
            } else {
                glm_mat4_mul(globals[parent], locals[i], globals[i]);
            }
            resolved[i] = 1;
        }
    }

    free(resolved);
}

int skeleton_find_bone(Skeleton* s, const char* name) {
    if (!s || !name) return -1;
    for (int i = 0; i < s->bone_count; i++) {
        if (s->bone_names[i] && strcmp(s->bone_names[i], name) == 0) {
            return i;
        }
    }
    return -1;
}

float skinned_angle_diff(float from, float to) {
    float diff = to - from;
    while (diff > (float)M_PI) diff -= 2.0f * (float)M_PI;
    while (diff < -(float)M_PI) diff += 2.0f * (float)M_PI;
    return diff;
}

float skinned_update_body_yaw(float body_yaw, float look_yaw, float dt) {
    const float head_limit = (float)M_PI * 0.1f;
    const float body_turn_speed = 64.0f;

    float diff = skinned_angle_diff(body_yaw, look_yaw);
    if (fabsf(diff) <= head_limit || dt <= 0.0f) {
        return body_yaw;
    }

    float target = look_yaw - copysignf(head_limit, diff);
    float step = body_turn_speed * dt;
    float delta = skinned_angle_diff(body_yaw, target);

    if (fabsf(delta) <= step) {
        return target;
    }

    return body_yaw + copysignf(step, delta);
}

float skinned_head_yaw_offset(float body_yaw, float look_yaw) {
    return skinned_angle_diff(body_yaw, look_yaw);
}

static void bone_apply_local_yaw_pitch(mat4 local, float pitch, float yaw) {
    if (fabsf(pitch) < 0.0001f && fabsf(yaw) < 0.0001f) return;

    mat4 rot_y, rot_x, delta, result;
    glm_mat4_identity(rot_y);
    glm_mat4_identity(rot_x);
    glm_rotate(rot_y, yaw, (vec3){0.0f, 1.0f, 0.0f});
    glm_rotate(rot_x, pitch, (vec3){-1.0f, 0.0f, 0.0f});
    glm_mat4_mul(rot_y, rot_x, delta);
    glm_mat4_mul(local, delta, result);
    glm_mat4_copy(result, local);
}

static void skinned_apply_look(Skeleton* skeleton, mat4* local_mats, const SkinnedLook* look) {
    if (!look || !look->enabled || look->head_bone < 0 || look->head_bone >= skeleton->bone_count) {
        return;
    }

    if (look->neck_bone >= 0 && look->neck_bone < skeleton->bone_count) {
        bone_apply_local_yaw_pitch(
            local_mats[look->neck_bone],
            look->head_pitch * 0.35f,
            look->head_yaw * 0.35f
        );
    }

    bone_apply_local_yaw_pitch(
        local_mats[look->head_bone],
        look->head_pitch * 0.65f,
        look->head_yaw * 0.65f
    );
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

static void vec3_keys_push(Vec3Key** keys, int* count, float time, vec3 value) {
    int n = *count;
    *keys = realloc(*keys, sizeof(Vec3Key) * (size_t)(n + 1));
    (*keys)[n].time = time;
    glm_vec3_copy(value, (*keys)[n].value);
    *count = n + 1;
}

static void quat_keys_push(QuatKey** keys, int* count, float time, vec4 value) {
    int n = *count;
    *keys = realloc(*keys, sizeof(QuatKey) * (size_t)(n + 1));
    (*keys)[n].time = time;
    glm_vec4_copy(value, (*keys)[n].value);
    *count = n + 1;
}

void animation_clip_destroy(AnimationClip* clip) {
    if (!clip) return;
    for (int i = 0; i < clip->track_count; i++) {
        free(clip->tracks[i].pos_keys);
        free(clip->tracks[i].rot_keys);
        free(clip->tracks[i].scl_keys);
    }
    if (clip->tracks) free(clip->tracks);
    free(clip);
}

void animation_clip_add_track(AnimationClip* clip, BoneTrack track) {
    clip->tracks = realloc(clip->tracks, sizeof(BoneTrack) * (clip->track_count + 1));
    clip->tracks[clip->track_count] = track;
    clip->track_count++;
}

void animation_track_add_pos_key(BoneTrack* track, float time, vec3 value) {
    vec3_keys_push(&track->pos_keys, &track->pos_count, time, value);
}

void animation_track_add_rot_key(BoneTrack* track, float time, vec4 value) {
    quat_norm(value);
    quat_keys_push(&track->rot_keys, &track->rot_count, time, value);
}

void animation_track_add_scl_key(BoneTrack* track, float time, vec3 value) {
    vec3_keys_push(&track->scl_keys, &track->scl_count, time, value);
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

static void sample_vec3_keys(Vec3Key* keys, int count, float time, const vec3 bind, vec3 out) {
    if (count == 0) {
        glm_vec3_copy(bind, out);
        return;
    }
    if (time <= keys[0].time) {
        glm_vec3_copy(keys[0].value, out);
        return;
    }
    if (time >= keys[count - 1].time) {
        glm_vec3_copy(keys[count - 1].value, out);
        return;
    }
    for (int i = 1; i < count; i++) {
        if (keys[i].time >= time) {
            float t = (time - keys[i - 1].time) / (keys[i].time - keys[i - 1].time);
            vec3_lerp(keys[i - 1].value, keys[i].value, t, out);
            return;
        }
    }
    glm_vec3_copy(bind, out);
}

static void sample_quat_keys(QuatKey* keys, int count, float time, const vec4 bind, vec4 out) {
    if (count == 0) {
        glm_vec4_copy(bind, out);
        return;
    }
    if (time <= keys[0].time) {
        glm_vec4_copy(keys[0].value, out);
        return;
    }
    if (time >= keys[count - 1].time) {
        glm_vec4_copy(keys[count - 1].value, out);
        return;
    }
    for (int i = 1; i < count; i++) {
        if (keys[i].time >= time) {
            float t = (time - keys[i - 1].time) / (keys[i].time - keys[i - 1].time);
            quat_slerp(keys[i - 1].value, keys[i].value, t, out);
            return;
        }
    }
    glm_vec4_copy(bind, out);
}

static void bind_pose_trs(const mat4 bind, vec3 pos, vec4 rot, vec3 scl) {
    pos[0] = bind[3][0];
    pos[1] = bind[3][1];
    pos[2] = bind[3][2];
    glm_mat4_quat(bind, rot);
    glm_vec3_one(scl);
}

static void sample_bone_track(BoneTrack* track, float time, const mat4 bind_offset, TransformKey* out) {
    vec3 bind_pos, bind_scl;
    vec4 bind_rot;
    bind_pose_trs(bind_offset, bind_pos, bind_rot, bind_scl);

    sample_vec3_keys(track->pos_keys, track->pos_count, time, bind_pos, out->position);
    sample_quat_keys(track->rot_keys, track->rot_count, time, bind_rot, out->rotation);
    sample_vec3_keys(track->scl_keys, track->scl_count, time, bind_scl, out->scale);
}

void anim_calc_bone_matrices(AnimState* state, Skeleton* skeleton, mat4* out_matrices) {
    if (!state->clip || !skeleton) return;
    
    for (int i = 0; i < skeleton->bone_count; i++) {
        glm_mat4_copy(skeleton->bone_offsets[i], out_matrices[i]);
    }
    
    for (int i = 0; i < state->clip->track_count; i++) {
        BoneTrack* track = &state->clip->tracks[i];
        if (track->bone_index >= skeleton->bone_count) continue;
        if (track->pos_count == 0 && track->rot_count == 0 && track->scl_count == 0) continue;

        TransformKey interp;
        sample_bone_track(track, state->current_time, skeleton->bone_offsets[track->bone_index], &interp);

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

    // 16 floats per vertex: pos3, uv2, normal3, boneIDs4, weights4
    vbo_create(&s->gpu.vbo, s->vertices, s->vertex_count * 16 * sizeof(float));
    glBindBuffer(GL_ARRAY_BUFFER, s->gpu.vbo.id);
    vbo_attr(0, 3, 16 * sizeof(float), 0);
    vbo_attr(1, 2, 16 * sizeof(float), 3);
    vbo_attr(2, 3, 16 * sizeof(float), 5);

    vbo_attr(3, 4, 16 * sizeof(float), 8);
    vbo_attr(4, 4, 16 * sizeof(float), 12);

    if (s->indices && s->index_count > 0) {
        ebo_create(&s->gpu.ebo, s->indices, (size_t)s->index_count * sizeof(int));
    } else {
        s->gpu.ebo.id = 0;
    }

    if (!s->gpu.index_type) {
        s->gpu.index_type = GL_UNSIGNED_INT;
    }

    glGenBuffers(1, &s->gpu.bone_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, s->gpu.bone_ubo);
    glBufferData(GL_UNIFORM_BUFFER, SKIN_MAX_BONES * sizeof(mat4), NULL, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void skinned_render(Skinned* s, Program* active_program, float delta_time, const SkinnedLook* look) {
    if (!s || !s->gpu.skeleton) return;


    int bone_count = s->gpu.skeleton->bone_count;
    if (bone_count <= 0) return;
    if (bone_count > SKIN_MAX_BONES) bone_count = SKIN_MAX_BONES;
    if (s->index_count < 3 || s->gpu.ebo.id == 0) return;

    mat4 model;

    glm_mat4_identity(model);

    glm_translate(model, s->pos);
    glm_rotate(model, s->rot[0], (vec3){1.0f, 0.0f, 0.0f});
    glm_rotate(model, s->rot[1], (vec3){0.0f, 1.0f, 0.0f});
    glm_rotate(model, s->rot[2], (vec3){0.0f, 0.0f, 1.0f});
    glm_scale(model, s->scale);

    mat4 model_world;
    glm_mat4_mul(model, s->node_transform, model_world);

    mat4* local_mats = malloc(sizeof(mat4) * bone_count);
    mat4* global_mats = malloc(sizeof(mat4) * bone_count);
    mat4* final_mats = malloc(sizeof(mat4) * bone_count);
    if (!local_mats || !global_mats || !final_mats) {
        free(local_mats);
        free(global_mats);
        free(final_mats);
        return;
    }

    if (s->gpu.anim) {
        anim_calc_bone_matrices(s->gpu.anim, s->gpu.skeleton, local_mats);
    } else {
        for (int i = 0; i < bone_count; i++) {
            glm_mat4_copy(s->gpu.skeleton->bone_offsets[i], local_mats[i]);
        }
    }
    skinned_apply_look(s->gpu.skeleton, local_mats, look);
    skeleton_calc_global_matrices(s->gpu.skeleton, local_mats, global_mats);
    anim_get_final_matrices(s->gpu.skeleton, global_mats, final_mats);

    mat4 bone_ubo_data[SKIN_MAX_BONES];
    for (int i = 0; i < SKIN_MAX_BONES; i++) {
        glm_mat4_identity(bone_ubo_data[i]);
    }
    for (int i = 0; i < bone_count; i++) {
        glm_mat4_copy(final_mats[i], bone_ubo_data[i]);
    }

    program_use(active_program);
    program_set_mat4(active_program, "model", (float*)model_world);

    glBindBuffer(GL_UNIFORM_BUFFER, s->gpu.bone_ubo);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, SKIN_MAX_BONES * sizeof(mat4), bone_ubo_data);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, s->gpu.bone_ubo);

    GLint cull_enabled = glIsEnabled(GL_CULL_FACE);
    glDisable(GL_CULL_FACE);

    vao_bind(&s->gpu.vao);
    glDrawElements(GL_TRIANGLES, s->index_count, s->gpu.index_type, NULL);

    if (cull_enabled) glEnable(GL_CULL_FACE);

    free(local_mats);
    free(global_mats);
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
    if (!s) return;
    vbo_free(&s->gpu.vbo);
    ebo_free(&s->gpu.ebo);
    vao_free(&s->gpu.vao);
    if (s->gpu.bone_ubo != 0) {
        glDeleteBuffers(1, &s->gpu.bone_ubo);
        s->gpu.bone_ubo = 0;
    }
    free(s->vertices);
    free(s->indices);
    // gpu.skeleton / gpu.anim are non owning; freed by GLTFModel / game code.
}