#include "utils/gltf.h"
#define CGLTF_IMPLEMENTATION // even with this helper library, the parser is still huge
#include "cgltf.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static int cmp_vec3_key(const void* a, const void* b) {
    float ta = ((const Vec3Key*)a)->time;
    float tb = ((const Vec3Key*)b)->time;
    return (ta > tb) - (ta < tb);
}

static int cmp_quat_key(const void* a, const void* b) {
    float ta = ((const QuatKey*)a)->time;
    float tb = ((const QuatKey*)b)->time;
    return (ta > tb) - (ta < tb);
}

static void sort_bone_track_keys(BoneTrack* track) {
    if (track->pos_count > 1) {
        qsort(track->pos_keys, (size_t)track->pos_count, sizeof(Vec3Key), cmp_vec3_key);
    }
    if (track->rot_count > 1) {
        qsort(track->rot_keys, (size_t)track->rot_count, sizeof(QuatKey), cmp_quat_key);
    }
    if (track->scl_count > 1) {
        qsort(track->scl_keys, (size_t)track->scl_count, sizeof(Vec3Key), cmp_vec3_key);
    }
}

static void mat4_from_cgltf(const cgltf_float* src, mat4 dst) {
    dst[0][0] = src[0]; dst[0][1] = src[1]; dst[0][2] = src[2]; dst[0][3] = src[3];
    dst[1][0] = src[4]; dst[1][1] = src[5]; dst[1][2] = src[6]; dst[1][3] = src[7];
    dst[2][0] = src[8]; dst[2][1] = src[9]; dst[2][2] = src[10]; dst[2][3] = src[11];
    dst[3][0] = src[12]; dst[3][1] = src[13]; dst[3][2] = src[14]; dst[3][3] = src[15];
} // gyattgpt did this mat4

static void vec3_from_cgltf(const cgltf_float* src, vec3 dst) {
    dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2];
}

static void quat_from_cgltf(const cgltf_float* src, vec4 dst) {
    dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2]; dst[3] = src[3];
}

static int find_bone_index(Skeleton* sk, const char* name) {
    if (!name || !sk) return -1;
    for (int i = 0; i < sk->bone_count; i++) {
        if (sk->bone_names[i] && strcmp(sk->bone_names[i], name) == 0) return i;
    }
    return -1;
}

static int build_skeleton(cgltf_data* data, cgltf_skin* skin, Skeleton** out_skeleton) {
    if (!data || !skin || !out_skeleton) {
        printf("build_skeleton: Invalid parameters\n");
        return 0;
    }
    
    int bone_count = (int)skin->joints_count;
    if (bone_count == 0) {
        printf("build_skeleton: No joints in skin\n");
        return 0;
    }
    
    printf("build_skeleton: Creating skeleton with %d bones\n", bone_count);
    
    Skeleton* sk = skeleton_create(bone_count);
    if (!sk) {
        printf("build_skeleton: Failed to create skeleton\n");
        return 0;
    }
    
    for (int i = 0; i < bone_count; i++) {
        cgltf_node* joint = skin->joints[i];
        if (joint && joint->name) {
            skeleton_set_bone_name(sk, i, joint->name);
            printf("  Bone %d: %s\n", i, joint->name);
        } else {
            char default_name[32];
            snprintf(default_name, sizeof(default_name), "bone_%d", i);
            skeleton_set_bone_name(sk, i, default_name);
            printf("  Bone %d: (unnamed)\n", i);
        }
    }
    
    if (skin->inverse_bind_matrices) {
        cgltf_accessor* inv_bind_acc = skin->inverse_bind_matrices;
        
        if (!inv_bind_acc->buffer_view) {
            printf("build_skeleton: Inverse bind matrices accessor has no buffer_view\n");
            skeleton_destroy(sk);
            return 0;
        }
        
        if (!inv_bind_acc->buffer_view->buffer) {
            printf("build_skeleton: Inverse bind matrices buffer_view has no buffer\n");
            skeleton_destroy(sk);
            return 0;
        }
        
        if (!inv_bind_acc->buffer_view->buffer->data) {
            printf("build_skeleton: Inverse bind matrices buffer has no data (buffers not loaded)\n");
            skeleton_destroy(sk);
            return 0;
        }
        
        if ((int)inv_bind_acc->count < bone_count) {
            printf("build_skeleton: Inverse bind matrices count (%d) < bone count (%d)\n", 
                   (int)inv_bind_acc->count, bone_count);
            skeleton_destroy(sk);
            return 0;
        }
        
        char* buffer_data = (char*)inv_bind_acc->buffer_view->buffer->data;
        size_t offset = inv_bind_acc->offset + inv_bind_acc->buffer_view->offset;
        cgltf_float* inv_bind_data = (cgltf_float*)(buffer_data + offset);
        
        for (int i = 0; i < bone_count; i++) {
            mat4 inv_bind;
            mat4_from_cgltf(inv_bind_data + i * 16, inv_bind);
            skeleton_set_inverse_bind_pose(sk, i, inv_bind);
        }
    } else {
        printf("build_skeleton: No inverse bind matrices, using identity\n");
        for (int i = 0; i < bone_count; i++) {
            mat4 identity;
            glm_mat4_identity(identity);
            skeleton_set_inverse_bind_pose(sk, i, identity);
        }
    }
    
    for (int i = 0; i < bone_count; i++) {
        cgltf_node* joint = skin->joints[i];
        mat4 local;
        glm_mat4_identity(local);
        
        if (joint->has_matrix) {
            mat4_from_cgltf(joint->matrix, local);
        } else {
            if (joint->has_translation) {
                glm_translate(local, (vec3){joint->translation[0], joint->translation[1], joint->translation[2]});
            }
            if (joint->has_rotation) {
                mat4 rot;
                vec4 q = {joint->rotation[0], joint->rotation[1], joint->rotation[2], joint->rotation[3]};
                glm_quat_mat4(q, rot);
                glm_mat4_mul(local, rot, local);
            }
            if (joint->has_scale) {
                glm_scale(local, (vec3){joint->scale[0], joint->scale[1], joint->scale[2]});
            }
        }
        skeleton_set_bone_offset(sk, i, local);

        int parent_index = -1;
        cgltf_node* parent = joint->parent;
        while (parent) {
            if (parent->name) {
                parent_index = find_bone_index(sk, parent->name);
                if (parent_index >= 0) break;
            }
            parent = parent->parent;
        }
        skeleton_set_parent(sk, i, parent_index);
    }
    
    *out_skeleton = sk;
    return 1;
}

static AnimationClip* build_animation(cgltf_data* data, cgltf_animation* anim, Skeleton* sk) {
    if (!data || !anim || !sk) {
        printf("build_animation: Invalid parameters\n");
        return NULL;
    }
    
    if (anim->samplers_count == 0 || anim->channels_count == 0) {
        printf("build_animation: Animation '%s' has no samplers or channels\n", 
               anim->name ? anim->name : "unnamed");
        return NULL;
    }
    
    printf("build_animation: Building animation '%s' with %d channels\n", 
           anim->name ? anim->name : "unnamed", (int)anim->channels_count);
    
    AnimationClip* clip = animation_clip_create(anim->name ? anim->name : "unnamed");
    if (!clip) return NULL;
    
    clip->ticks_per_second = 30.0f;
    
    for (cgltf_size c = 0; c < anim->channels_count; c++) {
        cgltf_animation_channel* channel = &anim->channels[c];
        cgltf_animation_sampler* sampler = channel->sampler;
        cgltf_node* target = channel->target_node;
        
        if (!target || !target->name) {
            printf("  Channel %d: Target node has no name, skipping\n", (int)c);
            continue;
        }
        
        int bone_idx = find_bone_index(sk, target->name);
        if (bone_idx == -1) {
            printf("  Channel %d: Bone '%s' not found in skeleton, skipping\n", (int)c, target->name);
            continue;
        }
        
        if (!sampler->input) {
            printf("  Channel %d: No time accessor\n", (int)c);
            continue;
        }
        
        if (!sampler->output) {
            printf("  Channel %d: No value accessor\n", (int)c);
            continue;
        }
        
        if (sampler->input->buffer_view && sampler->input->buffer_view->buffer && 
            !sampler->input->buffer_view->buffer->data) {
            printf("  Channel %d: Time buffer data not loaded\n", (int)c);
            continue;
        }
        
        if (sampler->output->buffer_view && sampler->output->buffer_view->buffer && 
            !sampler->output->buffer_view->buffer->data) {
            printf("  Channel %d: Value buffer data not loaded\n", (int)c);
            continue;
        }
        
        cgltf_accessor* time_acc = sampler->input;
        float* times = (float*)malloc(time_acc->count * sizeof(float));
        if (!times) {
            printf("  Channel %d: Failed to allocate times\n", (int)c);
            continue;
        }
        
        for (cgltf_size i = 0; i < time_acc->count; i++) {
            cgltf_accessor_read_float(time_acc, i, &times[i], 1);
        }
        
        cgltf_accessor* value_acc = sampler->output;
        int value_components = cgltf_num_components(value_acc->type);
        float* values = (float*)malloc(value_acc->count * value_components * sizeof(float));
        if (!values) {
            free(times);
            printf("  Channel %d: Failed to allocate values\n", (int)c);
            continue;
        }
        
        for (cgltf_size i = 0; i < value_acc->count; i++) {
            cgltf_accessor_read_float(value_acc, i, values + i * value_components, value_components);
        }
        
        BoneTrack* track = NULL;
        for (int t = 0; t < clip->track_count; t++) {
            if (clip->tracks[t].bone_index == bone_idx) {
                track = &clip->tracks[t];
                break;
            }
        }
        
        if (!track) {
            BoneTrack new_track = {0};
            new_track.bone_index = bone_idx;
            strncpy(new_track.bone_name, target->name, sizeof(new_track.bone_name) - 1);
            animation_clip_add_track(clip, new_track);
            track = &clip->tracks[clip->track_count - 1];
        }
        
        for (cgltf_size k = 0; k < time_acc->count; k++) {
            float t = times[k];
            if (channel->target_path == cgltf_animation_path_type_translation) {
                vec3 v;
                vec3_from_cgltf(values + k * value_components, v);
                animation_track_add_pos_key(track, t, v);
            } else if (channel->target_path == cgltf_animation_path_type_rotation) {
                vec4 q;
                quat_from_cgltf(values + k * value_components, q);
                animation_track_add_rot_key(track, t, q);
            } else if (channel->target_path == cgltf_animation_path_type_scale) {
                vec3 s;
                vec3_from_cgltf(values + k * value_components, s);
                animation_track_add_scl_key(track, t, s);
            }
            if (t > clip->duration) clip->duration = t;
        }
        
        free(times);
        free(values);
    }
    
    for (int t = 0; t < clip->track_count; t++) {
        sort_bone_track_keys(&clip->tracks[t]);
    }

    printf("  Animation duration: %.2f seconds\n", clip->duration);
    
    return clip;
}

static Skinned* build_skinned_mesh(cgltf_data* data, cgltf_mesh* mesh, cgltf_skin* skin, Skeleton* sk) {
    if (!data || !mesh || !sk) {
        printf("build_skinned_mesh: Invalid parameters\n");
        return NULL;
    }
    
    if (mesh->primitives_count == 0) {
        printf("build_skinned_mesh: Mesh has no primitives\n");
        return NULL;
    }
    
    cgltf_primitive* prim = &mesh->primitives[0];
    
    int vertex_count = 0;
    cgltf_accessor* pos_acc = NULL;
    
    for (cgltf_size a = 0; a < prim->attributes_count; a++) {
        if (prim->attributes[a].type == cgltf_attribute_type_position) {
            pos_acc = prim->attributes[a].data;
            if (pos_acc) {
                vertex_count = (int)pos_acc->count;
            }
            break;
        }
    }
    
    if (vertex_count == 0 || !pos_acc) {
        printf("build_skinned_mesh: No position data found\n");
        return NULL;
    }
    
    int index_count = 0;
    if (prim->indices) {
        index_count = (int)prim->indices->count;
    }
    
    printf("build_skinned_mesh: Building mesh with %d vertices, %d indices\n", vertex_count, index_count);
    
    float* vertices = (float*)calloc((size_t)vertex_count, 16 * sizeof(float));
    if (!vertices) {
        printf("build_skinned_mesh: Failed to allocate vertices\n");
        return NULL;
    }
    
    int* indices = NULL;
    if (index_count > 0) {
        indices = (int*)malloc((size_t)index_count * sizeof(int));
        if (!indices) {
            printf("build_skinned_mesh: Failed to allocate indices\n");
            free(vertices);
            return NULL;
        }
    }
    
    for (int i = 0; i < vertex_count; i++) {
        vertices[i * 16 + 5] = 0.0f;
        vertices[i * 16 + 6] = 1.0f;
        vertices[i * 16 + 7] = 0.0f;
        vertices[i * 16 + 12] = 1.0f;
    }
    
    if (pos_acc) {
        int read_n = vertex_count;
        if ((int)pos_acc->count < read_n) read_n = (int)pos_acc->count;
        for (int i = 0; i < read_n; i++) {
            float pos[3];
            cgltf_accessor_read_float(pos_acc, i, pos, 3);
            vertices[i * 16 + 0] = pos[0];
            vertices[i * 16 + 1] = pos[1];
            vertices[i * 16 + 2] = pos[2];
        }
    }
    
    int has_joints = 0;
    int has_weights = 0;
    
    for (cgltf_size a = 0; a < prim->attributes_count; a++) {
        cgltf_attribute_type attr_type = prim->attributes[a].type;
        cgltf_accessor* acc = prim->attributes[a].data;
        
        if (!acc) continue;
        
        int read_n = vertex_count;
        if ((int)acc->count < read_n) read_n = (int)acc->count;
        
        if (attr_type == cgltf_attribute_type_normal) {
            for (int i = 0; i < read_n; i++) {
                float norm[3];
                cgltf_accessor_read_float(acc, i, norm, 3);
                vertices[i * 16 + 5] = norm[0];
                vertices[i * 16 + 6] = norm[1];
                vertices[i * 16 + 7] = norm[2];
            }
        } else if (attr_type == cgltf_attribute_type_texcoord) {
            for (int i = 0; i < read_n; i++) {
                float uv[2];
                cgltf_accessor_read_float(acc, i, uv, 2);
                vertices[i * 16 + 3] = uv[0];
                vertices[i * 16 + 4] = uv[1];
            }
        } else if (attr_type == cgltf_attribute_type_joints) {
            has_joints = 1;
            for (int i = 0; i < read_n; i++) {
                cgltf_uint joints[4] = {0};
                cgltf_accessor_read_uint(acc, i, joints, 4);
                vertices[i * 16 + 8] = (float)joints[0];
                vertices[i * 16 + 9] = (float)joints[1];
                vertices[i * 16 + 10] = (float)joints[2];
                vertices[i * 16 + 11] = (float)joints[3];
            }
        } else if (attr_type == cgltf_attribute_type_weights) {
            has_weights = 1;
            for (int i = 0; i < read_n; i++) {
                float weights[4] = {0};
                cgltf_accessor_read_float(acc, i, weights, 4);
                vertices[i * 16 + 12] = weights[0];
                vertices[i * 16 + 13] = weights[1];
                vertices[i * 16 + 14] = weights[2];
                vertices[i * 16 + 15] = weights[3];
            }
        }
    }
    
    if (!has_joints || !has_weights) {
        printf("build_skinned_mesh: Warning - Mesh has no joint/weight data, binding to bone 0\n");
    }
    
    if (prim->indices && indices) {
        cgltf_accessor* idx_acc = prim->indices;
        int read_n = index_count;
        if ((int)idx_acc->count < read_n) read_n = (int)idx_acc->count;
        for (int i = 0; i < read_n; i++) {
            cgltf_uint idx;
            cgltf_accessor_read_uint(idx_acc, i, &idx, 1);
            indices[i] = (int)idx;
        }
    }
    
    Skinned* skinned = (Skinned*)malloc(sizeof(Skinned));
    if (!skinned) {
        printf("build_skinned_mesh: Failed to allocate Skinned\n");
        free(vertices);
        free(indices);
        return NULL;
    }
    
    memset(skinned, 0, sizeof(Skinned));
    
    skinned->vertices = vertices;
    skinned->indices = indices;
    skinned->vertex_count = vertex_count;
    skinned->index_count = index_count;
    
    glm_vec3_zero(skinned->pos);
    glm_vec3_zero(skinned->rot);
    glm_vec3_one(skinned->scale);
    glm_mat4_identity(skinned->node_transform);

    skinned->gpu.skeleton = sk;
    skinned_cache(skinned);
    
    printf("build_skinned_mesh: Successfully created skinned mesh\n");
    
    return skinned;
}

int gltf_load(const char* path, GLTFModel* out) {
    if (!path || !out) {
        printf("gltf_load: Invalid parameters\n");
        return 0;
    }
    
    memset(out, 0, sizeof(GLTFModel));
    
    printf("gltf_load: Loading %s\n", path);
    
    cgltf_options options = {0};
    cgltf_data* data = NULL;
    cgltf_result result = cgltf_parse_file(&options, path, &data);
    
    if (result != cgltf_result_success) {
        printf("gltf_load: Failed to parse GLTF/GLB file: %s\n", path);
        return 0;
    }
    
    printf("gltf_load: Parsed successfully. Loading buffers...\n");
    
    result = cgltf_load_buffers(&options, data, path);
    if (result != cgltf_result_success) {
        printf("gltf_load: Failed to load buffers (error code: %d)\n", result);
        cgltf_free(data);
        return 0;
    }
    
    printf("gltf_load: Buffers loaded. Meshes: %d, Skins: %d, Animations: %d\n",
           (int)data->meshes_count, (int)data->skins_count, (int)data->animations_count);
    
    for (cgltf_size i = 0; i < data->buffers_count; i++) {
        if (!data->buffers[i].data) {
            printf("gltf_load: Warning - Buffer %zu has no data\n", i);
        } else {
            printf("gltf_load: Buffer %zu size: %zu bytes\n", i, data->buffers[i].size);
        }
    }
    
    if (data->skins_count == 0) {
        printf("gltf_load: No skins found in file\n");
        cgltf_free(data);
        return 0;
    }
    
    if (data->meshes_count == 0) {
        printf("gltf_load: No meshes found in file\n");
        cgltf_free(data);
        return 0;
    }
    
    cgltf_mesh* target_mesh = &data->meshes[0];
    cgltf_skin* target_skin = &data->skins[0];
    
    printf("gltf_load: Building skeleton...\n");
    
    if (!build_skeleton(data, target_skin, &out->skeleton)) {
        printf("gltf_load: Failed to build skeleton\n");
        cgltf_free(data);
        return 0;
    }
    
    printf("gltf_load: Building skinned mesh...\n");
    
    out->skinned = build_skinned_mesh(data, target_mesh, target_skin, out->skeleton);
    if (!out->skinned) {
        printf("gltf_load: Failed to build skinned mesh\n");
        skeleton_destroy(out->skeleton);
        out->skeleton = NULL;
        cgltf_free(data);
        return 0;
    }

    cgltf_node* mesh_node = NULL;
    for (cgltf_size n = 0; n < data->nodes_count; n++) {
        if (data->nodes[n].mesh == target_mesh) {
            mesh_node = &data->nodes[n];
            break;
        }
    }
    if (mesh_node) {
        cgltf_float world_mat[16];
        cgltf_node_transform_world(mesh_node, world_mat);
        mat4_from_cgltf(world_mat, out->skinned->node_transform);
    }
    
    out->animation_count = (int)data->animations_count; // pls just work i am here suffering
    if (out->animation_count > 0) {
        out->animations = (AnimationClip**)calloc((size_t)out->animation_count, sizeof(AnimationClip*));
        out->animation_names = (char**)calloc((size_t)out->animation_count, sizeof(char*));
        
        if (out->animations && out->animation_names) {
            for (int i = 0; i < out->animation_count; i++) {
                printf("gltf_load: Building animation %d/%d\n", i + 1, out->animation_count);
                out->animations[i] = build_animation(data, &data->animations[i], out->skeleton);
                if (out->animations[i]) {
                    out->animation_names[i] = (char*)malloc(64);
                    if (out->animation_names[i]) {
                        strncpy(out->animation_names[i], out->animations[i]->name, 63);
                        out->animation_names[i][63] = '\0';
                    }
                }
            }
        } else {
            printf("gltf_load: Failed to allocate animation arrays\n");
        }
    }
    
    cgltf_free(data);
    
    printf("gltf_load: Successfully loaded model with %d bones, %d animations\n",
           out->skeleton ? out->skeleton->bone_count : 0, out->animation_count);
    
    return 1;
}

AnimationClip* gltf_get_animation(GLTFModel* model, const char* name) {
    if (!model || !name) return NULL;
    
    for (int i = 0; i < model->animation_count; i++) {
        if (model->animation_names[i] && strcmp(model->animation_names[i], name) == 0) {
            return model->animations[i];
        }
    }
    return NULL;
}

void gltf_free(GLTFModel* model) {
    if (!model) return;
    
    if (model->skinned) {
        skinned_destroy(model->skinned);
        free(model->skinned);
        model->skinned = NULL;
    }
    
    if (model->skeleton) {
        skeleton_destroy(model->skeleton);
        model->skeleton = NULL;
    }
    
    if (model->animations) {
        for (int i = 0; i < model->animation_count; i++) {
            if (model->animations[i]) {
                animation_clip_destroy(model->animations[i]);
                model->animations[i] = NULL;
            }
            if (model->animation_names[i]) {
                free(model->animation_names[i]);
                model->animation_names[i] = NULL;
            }
        }
        free(model->animations);
        free(model->animation_names);
        model->animations = NULL;
        model->animation_names = NULL;
    }
    
    model->animation_count = 0;
}