#include "physics/softbody.h"
#include "core/gfx.h"
#include "core/skin.h"
#include "utils/mesh_raw.h"
#include "utils/obj.h"
#include "utils/rand.h"
#include "core/world.h"
#include "core/tile.h"

#include <cglm/cglm.h>
#include <cglm/quat.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>

#ifdef SOFTBODY_USE_GLTF
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#endif

#define SUBSTEPS 16
#define MAX_BONE_RADIUS 0.15f

typedef struct {
    vec3 position;
    vec3 velocity;
    int   pinned;
    versor rotation;
} SoftbodyBoneNode;

typedef struct {
    int   bone_a, bone_b;
    float rest_length;
} SoftbodySpring;

struct Softbody {
    float* vertices;
    int    vertex_count;
    int*   indices;
    int    index_count;
    vec3*          rest_positions;
    SoftbodyBoneNode* bones;
    int bone_count;
    int grid_x, grid_y, grid_z;
    SoftbodySpring* springs;
    int spring_count;
    Skeleton* skeleton;
    Skinned*  skinned;
    float* skin_weights;
    SoftbodyConfig config;
    vec3 pos, rot, scale;
    mat4 model;
    mat4 inv_model;
    mat4 inv_rot_model;
    vec3 rest_center;
    int center_bone_index;
};

static float vec3_dist_sq(const vec3 a, const vec3 b) {
    float dx = a[0] - b[0];
    float dy = a[1] - b[1];
    float dz = a[2] - b[2];
    return dx*dx + dy*dy + dz*dz;
}

static int is_solid_block(World* world, int x, int y, int z) {
    uint16_t b = world_get_block(world, x, y, z);
    return b != AIR && lookup_ignorecollision[b] == 0;
}

static void resolve_collision(World* world, vec3 pos, vec3 vel, float radius, float bounce) {
    int min_x = (int)floorf(pos[0] - radius);
    int max_x = (int)floorf(pos[0] + radius);
    int min_y = (int)floorf(pos[1] - radius);
    int max_y = (int)floorf(pos[1] + radius);
    int min_z = (int)floorf(pos[2] - radius);
    int max_z = (int)floorf(pos[2] + radius);
    int hit = 0;
    
    for (int cx = min_x; cx <= max_x; cx++) {
        for (int cy = min_y; cy <= max_y; cy++) {
            for (int cz = min_z; cz <= max_z; cz++) {
                if (!is_solid_block(world, cx, cy, cz)) continue;
                hit = 1;
                float half = 0.5f;
                float block_cx = (float)cx + half;
                float block_cy = (float)cy + half;
                float block_cz = (float)cz + half;
                
                float closest_x = pos[0];
                float closest_y = pos[1];
                float closest_z = pos[2];
                
                if (closest_x < block_cx - half) closest_x = block_cx - half;
                if (closest_x > block_cx + half) closest_x = block_cx + half;
                if (closest_y < block_cy - half) closest_y = block_cy - half;
                if (closest_y > block_cy + half) closest_y = block_cy + half;
                if (closest_z < block_cz - half) closest_z = block_cz - half;
                if (closest_z > block_cz + half) closest_z = block_cz + half;
                
                float dx = pos[0] - closest_x;
                float dy = pos[1] - closest_y;
                float dz = pos[2] - closest_z;
                float dist_sq = dx*dx + dy*dy + dz*dz;
                
                if (dist_sq < radius * radius && dist_sq > 0.000001f) {
                    float dist = sqrtf(dist_sq);
                    float pen = radius - dist;
                    
                    float nx = dx / dist;
                    float ny = dy / dist;
                    float nz = dz / dist;
                    
                    pos[0] += nx * pen;
                    pos[1] += ny * pen;
                    pos[2] += nz * pen;
                    
                    float vel_normal = vel[0]*nx + vel[1]*ny + vel[2]*nz;
                    
                    if (vel_normal < 0) {
                        vel[0] -= (1.0f + bounce) * vel_normal * nx;
                        vel[1] -= (1.0f + bounce) * vel_normal * ny;
                        vel[2] -= (1.0f + bounce) * vel_normal * nz;
                    }
                }
                else if (dist_sq < 0.000001f) {
                    float dx_c = pos[0] - block_cx;
                    float dy_c = pos[1] - block_cy;
                    float dz_c = pos[2] - block_cz;
                    float ax = fabsf(dx_c), ay = fabsf(dy_c), az = fabsf(dz_c);
                    float face_x = half + radius + 0.001f;
                    float face_y = half + radius + 0.001f;
                    float face_z = half + radius + 0.001f;
                    if (ax >= ay && ax >= az) {
                        pos[0] = block_cx + (dx_c >= 0 ? face_x : -face_x);
                    } else if (ay >= az) {
                        pos[1] = block_cy + (dy_c >= 0 ? face_y : -face_y);
                    } else {
                        pos[2] = block_cz + (dz_c >= 0 ? face_z : -face_z);
                    }
                    vel[0] = 0;
                    vel[1] = 0;
                    vel[2] = 0;
                }
            }
        }
    }
    if (hit) {
        vel[0] *= 0.95f;
        vel[1] *= 0.95f;
        vel[2] *= 0.95f;
    }
}

static int load_mesh_vertices(const char* path,
                               float** out_vertices, int* out_vcount,
                               int** out_indices,    int* out_icount) {
    Render_request* req = obj_load_render_request(path);
    if (req) {
        int vc = req->data_size / (8 * (int)sizeof(float));
        *out_vertices = (float*)malloc((size_t)vc * 8 * sizeof(float));
        if (!*out_vertices) { free(req->data); free(req->triangles); free(req); return 0; }
        memcpy(*out_vertices, req->data, (size_t)vc * 8 * sizeof(float));
        *out_vcount = vc;

        int ic = req->tri_count * 3;
        *out_indices = (int*)malloc((size_t)ic * sizeof(int));
        if (!*out_indices) { free(*out_vertices); free(req->data); free(req->triangles); free(req); return 0; }
        memcpy(*out_indices, req->triangles, (size_t)ic * sizeof(int));
        *out_icount = ic;

        free(req->data);
        free(req->triangles);
        free(req);
        return 1;
    }

#ifdef SOFTBODY_USE_GLTF
    cgltf_options options = {0};
    cgltf_data* data = NULL;
    if (cgltf_parse_file(&options, path, &data) != cgltf_result_success)
        return 0;
    if (cgltf_load_buffers(&options, data, path) != cgltf_result_success) {
        cgltf_free(data);
        return 0;
    }
    if (data->meshes_count == 0 || data->meshes[0].primitives_count == 0) {
        cgltf_free(data);
        return 0;
    }

    cgltf_primitive* prim = &data->meshes[0].primitives[0];
    cgltf_accessor* pos_acc = NULL;
    cgltf_accessor* uv_acc = NULL;
    cgltf_accessor* nrm_acc = NULL;

    for (cgltf_size a = 0; a < prim->attributes_count; a++) {
        if (prim->attributes[a].type == cgltf_attribute_type_position)
            pos_acc = prim->attributes[a].data;
        if (prim->attributes[a].type == cgltf_attribute_type_texcoord)
            uv_acc = prim->attributes[a].data;
        if (prim->attributes[a].type == cgltf_attribute_type_normal)
            nrm_acc = prim->attributes[a].data;
    }

    if (!pos_acc) { cgltf_free(data); return 0; }

    int vc = (int)pos_acc->count;
    float* verts = (float*)malloc((size_t)vc * 8 * sizeof(float));
    if (!verts) { cgltf_free(data); return 0; }

    for (int i = 0; i < vc; i++) {
        float p[3]={0}, uv[2]={0}, n[3]={0,1,0};
        cgltf_accessor_read_float(pos_acc, i, p, 3);
        if (uv_acc) cgltf_accessor_read_float(uv_acc, i, uv, 2);
        if (nrm_acc) cgltf_accessor_read_float(nrm_acc, i, n, 3);
        verts[i*8+0] = p[0]; verts[i*8+1] = p[1]; verts[i*8+2] = p[2];
        verts[i*8+3] = uv[0]; verts[i*8+4] = uv[1];
        verts[i*8+5] = n[0]; verts[i*8+6] = n[1]; verts[i*8+7] = n[2];
    }

    int ic = 0;
    int* idx = NULL;
    if (prim->indices) {
        ic = (int)prim->indices->count;
        idx = (int*)malloc((size_t)ic * sizeof(int));
        if (idx) {
            for (int i = 0; i < ic; i++) {
                cgltf_uint v;
                cgltf_accessor_read_uint(prim->indices, i, &v, 1);
                idx[i] = (int)v;
            }
        }
    } else {
        ic = vc;
        idx = (int*)malloc((size_t)ic * sizeof(int));
        if (idx) {
            for (int i = 0; i < ic; i++) idx[i] = i;
        }
    }

    *out_vertices = verts;
    *out_vcount   = vc;
    *out_indices  = idx;
    *out_icount   = ic;

    cgltf_free(data);
    return 1;
#else
    (void)path;
    (void)out_vertices; (void)out_vcount;
    (void)out_indices;  (void)out_icount;
    return 0;
#endif
}

static void compute_skin_weights(const float* verts, int vcount,
                                  const vec3* rest_pos, int bone_count,
                                  float* out_weights) {
    for (int vi = 0; vi < vcount; vi++) {
        vec3 vp = { verts[vi*8+0], verts[vi*8+1], verts[vi*8+2] };

        typedef struct { int idx; float dist_sq; } Dist;
        Dist dists[256];
        int n = bone_count < 256 ? bone_count : 256;
        for (int bi = 0; bi < n; bi++) {
            dists[bi].idx = bi;
            dists[bi].dist_sq = vec3_dist_sq(vp, rest_pos[bi]);
        }

        int k = 4;
        if (n < k) k = n;
        for (int i = 0; i < k; i++) {
            int best = i;
            for (int j = i + 1; j < n; j++) {
                if (dists[j].dist_sq < dists[best].dist_sq)
                    best = j;
            }
            Dist tmp = dists[i];
            dists[i] = dists[best];
            dists[best] = tmp;
        }

        float wsum = 0.0f;
        float ws[4] = {0};
        for (int i = 0; i < k; i++) {
            float d = sqrtf(dists[i].dist_sq);
            if (d < 0.0001f) {
                ws[i] = 1.0f;
                wsum = 1.0f;
                for (int j = 0; j < k; j++) if (j != i) ws[j] = 0.0f;
                break;
            }
            ws[i] = 1.0f / d;
            wsum += ws[i];
        }
        if (wsum > 0.0f) {
            for (int i = 0; i < 4; i++) {
                if (i < k) ws[i] /= wsum;
                else ws[i] = 0.0f;
            }
        }

        for (int i = k; i < 4; i++) {
            dists[i].idx = 0;
            ws[i] = 0.0f;
        }

        for (int i = 0; i < 4; i++) {
            out_weights[vi * 8 + i]     = (float)dists[i].idx;
            out_weights[vi * 8 + 4 + i] = ws[i];
        }
    }
}

static float* build_skinned_vertices(const float* src_verts, int vcount,
                                      const float* skin_weights,
                                      int* out_vcount) {
    float* dst = (float*)calloc((size_t)vcount, 16 * sizeof(float));
    if (!dst) { *out_vcount = 0; return NULL; }

    for (int i = 0; i < vcount; i++) {
        dst[i*16+0] = src_verts[i*8+0];
        dst[i*16+1] = src_verts[i*8+1];
        dst[i*16+2] = src_verts[i*8+2];
        dst[i*16+3] = src_verts[i*8+3];
        dst[i*16+4] = src_verts[i*8+4];
        dst[i*16+5] = src_verts[i*8+5];
        dst[i*16+6] = src_verts[i*8+6];
        dst[i*16+7] = src_verts[i*8+7];
        for (int j = 0; j < 4; j++)
            dst[i*16+8+j] = skin_weights[i*8+j];
        for (int j = 0; j < 4; j++)
            dst[i*16+12+j] = skin_weights[i*8+4+j];
    }

    *out_vcount = vcount;
    return dst;
}

static Skeleton* softbody_build_skeleton(const vec3* rest_pos, int bone_count) {
    Skeleton* sk = skeleton_create(bone_count);
    if (!sk) return NULL;

    for (int i = 0; i < bone_count; i++) {
        char name[32];
        snprintf(name, sizeof(name), "softbody_%d", i);
        skeleton_set_bone_name(sk, i, name);
        skeleton_set_parent(sk, i, -1);

        mat4 offset;
        glm_mat4_identity(offset);
        glm_translate(offset, rest_pos[i]);
        skeleton_set_bone_offset(sk, i, offset);

        mat4 inv_bind;
        glm_mat4_identity(inv_bind);
        glm_translate(inv_bind, (vec3){ -rest_pos[i][0],
                                        -rest_pos[i][1],
                                        -rest_pos[i][2] });
        skeleton_set_inverse_bind_pose(sk, i, inv_bind);
    }

    return sk;
}

static void rebuild_model_matrix(Softbody* sb) {
    glm_mat4_identity(sb->model);
    glm_translate(sb->model, sb->pos);
    glm_rotate(sb->model, sb->rot[0], (vec3){1,0,0});
    glm_rotate(sb->model, sb->rot[1], (vec3){0,1,0});
    glm_rotate(sb->model, sb->rot[2], (vec3){0,0,1});

    mat4 rot_only;
    glm_mat4_identity(rot_only);
    glm_rotate(rot_only, sb->rot[0], (vec3){1,0,0});
    glm_rotate(rot_only, sb->rot[1], (vec3){0,1,0});
    glm_rotate(rot_only, sb->rot[2], (vec3){0,0,1});
    glm_mat4_inv(rot_only, sb->inv_rot_model);

    glm_scale(sb->model, sb->scale);
    glm_mat4_inv(sb->model, sb->inv_model);
}

static void add_spring(SoftbodySpring** springs, int* count,
                        int ba, int bb, float rest) {
    (*count)++;
    *springs = (SoftbodySpring*)realloc(*springs,
                                         (size_t)(*count) * sizeof(SoftbodySpring));
    if (!*springs) { *count = 0; return; }
    SoftbodySpring* s = &(*springs)[*count - 1];
    s->bone_a      = ba;
    s->bone_b      = bb;
    s->rest_length = rest;
}

Softbody* softbody_load(const char* path, const SoftbodyConfig* cfg) {
    if (!path) return NULL;

    float* raw_verts = NULL;
    int    raw_vcount = 0;
    int*   raw_indices = NULL;
    int    raw_icount = 0;

    if (!load_mesh_vertices(path, &raw_verts, &raw_vcount,
                             &raw_indices, &raw_icount)) {
        printf("softbody_load: Failed to load mesh '%s'\n", path);
        return NULL;
    }

    SoftbodyConfig conf;
    if (cfg) {
        memcpy(&conf, cfg, sizeof(conf));
    } else {
        memset(&conf, 0, sizeof(conf));
    }
    if (conf.bone_count <= 0) conf.bone_count = 64;
    if (conf.spring_k <= 0.0f) conf.spring_k = 300.0f;
    if (conf.damping <= 0.0f)  conf.damping = 3.0f;
    if (conf.gravity == 0.0f)  conf.gravity = -20.0f;
    if (conf.bounce_factor <= 0.0f) conf.bounce_factor = 0.15f;

    int target_bones = conf.bone_count;
    if (target_bones > raw_vcount) target_bones = raw_vcount;

    Softbody* sb = (Softbody*)calloc(1, sizeof(Softbody));
    if (!sb) { free(raw_verts); free(raw_indices); return NULL; }

    sb->pos[0] = sb->pos[1] = sb->pos[2] = 0.0f;
    sb->rot[0] = sb->rot[1] = sb->rot[2] = 0.0f;
    sb->scale[0] = sb->scale[1] = sb->scale[2] = 1.0f;
    memcpy(&sb->config, &conf, sizeof(conf));

    int* sampled_indices = (int*)malloc((size_t)target_bones * sizeof(int));
    if (!sampled_indices) { softbody_destroy(sb); free(raw_verts); free(raw_indices); return NULL; }

    int first = (int)(RANDF() * raw_vcount);
    if (first >= raw_vcount) first = raw_vcount - 1;
    sampled_indices[0] = first;

    float* min_dists = (float*)malloc((size_t)raw_vcount * sizeof(float));
    if (!min_dists) { free(sampled_indices); softbody_destroy(sb); free(raw_verts); free(raw_indices); return NULL; }
    for (int i = 0; i < raw_vcount; i++) {
        vec3 v = { raw_verts[i*8+0], raw_verts[i*8+1], raw_verts[i*8+2] };
        vec3 s = { raw_verts[first*8+0], raw_verts[first*8+1], raw_verts[first*8+2] };
        min_dists[i] = vec3_dist_sq(v, s);
    }

    for (int j = 1; j < target_bones; j++) {
        int best_idx = 0;
        float best_dist = -1.0f;
        for (int i = 0; i < raw_vcount; i++) {
            if (min_dists[i] > best_dist) {
                best_dist = min_dists[i];
                best_idx = i;
            }
        }
        sampled_indices[j] = best_idx;
        vec3 sp = { raw_verts[best_idx*8+0], raw_verts[best_idx*8+1], raw_verts[best_idx*8+2] };
        for (int i = 0; i < raw_vcount; i++) {
            vec3 vp = { raw_verts[i*8+0], raw_verts[i*8+1], raw_verts[i*8+2] };
            float d = vec3_dist_sq(vp, sp);
            if (d < min_dists[i]) min_dists[i] = d;
        }
    }
    free(min_dists);

    int total_bones = target_bones + 1;
    sb->bone_count = total_bones;
    sb->rest_positions = (vec3*)calloc((size_t)total_bones, sizeof(vec3));
    sb->bones = (SoftbodyBoneNode*)calloc((size_t)total_bones, sizeof(SoftbodyBoneNode));
    if (!sb->rest_positions || !sb->bones) {
        free(sampled_indices);
        softbody_destroy(sb);
        free(raw_verts);
        free(raw_indices);
        return NULL;
    }

    vec3 center = {0,0,0};
    for (int i = 0; i < target_bones; i++) {
        int vi = sampled_indices[i];
        sb->rest_positions[i][0] = raw_verts[vi*8+0];
        sb->rest_positions[i][1] = raw_verts[vi*8+1];
        sb->rest_positions[i][2] = raw_verts[vi*8+2];
        glm_vec3_copy(sb->rest_positions[i], sb->bones[i].position);
        glm_vec3_zero(sb->bones[i].velocity);
        sb->bones[i].pinned = 0;
        glm_quat_identity(sb->bones[i].rotation);
        glm_vec3_add(center, sb->rest_positions[i], center);
    }
    glm_vec3_scale(center, 1.0f / target_bones, sb->rest_center);
    free(sampled_indices);

    int center_idx = target_bones;
    glm_vec3_copy(sb->rest_center, sb->rest_positions[center_idx]);
    glm_vec3_copy(sb->rest_center, sb->bones[center_idx].position);
    glm_vec3_zero(sb->bones[center_idx].velocity);
    sb->bones[center_idx].pinned = 0;
    glm_quat_identity(sb->bones[center_idx].rotation);
    sb->center_bone_index = center_idx;

    float* skin_w = (float*)calloc((size_t)raw_vcount, 8 * sizeof(float));
    if (!skin_w) {
        softbody_destroy(sb);
        free(raw_verts);
        free(raw_indices);
        return NULL;
    }
    compute_skin_weights(raw_verts, raw_vcount, sb->rest_positions, target_bones, skin_w);
    sb->skin_weights = skin_w;

    sb->springs = NULL;
    sb->spring_count = 0;

    float max_dist_sq = 0.0f;
    for (int i = 0; i < target_bones; i++) {
        for (int j = i + 1; j < target_bones; j++) {
            float d = vec3_dist_sq(sb->rest_positions[i], sb->rest_positions[j]);
            if (d > max_dist_sq) max_dist_sq = d;
        }
    }
    float max_spring_dist = sqrtf(max_dist_sq) * 0.7f;
    float max_spring_dist_sq = max_spring_dist * max_spring_dist;

    for (int i = 0; i < target_bones; i++) {
        for (int j = i + 1; j < target_bones; j++) {
            float dist_sq = vec3_dist_sq(sb->rest_positions[i], sb->rest_positions[j]);
            if (dist_sq < max_spring_dist_sq) {
                add_spring(&sb->springs, &sb->spring_count, i, j, sqrtf(dist_sq));
            }
        }
    }

    for (int i = 0; i < target_bones; i++) {
        float dist = sqrtf(vec3_dist_sq(sb->rest_positions[i], sb->rest_positions[center_idx]));
        add_spring(&sb->springs, &sb->spring_count, i, center_idx, dist);
    }

    sb->grid_x = sb->bone_count;
    sb->grid_y = 1;
    sb->grid_z = 1;

    sb->vertex_count = 0;
    sb->vertices = build_skinned_vertices(raw_verts, raw_vcount, skin_w, &sb->vertex_count);
    sb->indices = raw_indices;
    sb->index_count = raw_icount;
    free(raw_verts);

    sb->skeleton = softbody_build_skeleton(sb->rest_positions, target_bones);
    if (!sb->skeleton) { softbody_destroy(sb); return NULL; }

    sb->skinned = (Skinned*)calloc(1, sizeof(Skinned));
    if (!sb->skinned) { softbody_destroy(sb); return NULL; }
    sb->skinned->vertices     = sb->vertices;
    sb->skinned->vertex_count = sb->vertex_count;
    sb->skinned->indices      = sb->indices;
    sb->skinned->index_count  = sb->index_count;
    glm_vec3_zero(sb->skinned->pos);
    glm_vec3_zero(sb->skinned->rot);
    glm_vec3_one(sb->skinned->scale);
    glm_mat4_identity(sb->skinned->node_transform);
    sb->skinned->gpu.skeleton = sb->skeleton;
    sb->skinned->gpu.anim     = NULL;
    skinned_cache(sb->skinned);

    rebuild_model_matrix(sb);

    return sb;
}

void softbody_destroy(Softbody* sb) {
    if (!sb) return;
    if (sb->skinned) {
        sb->skinned->vertices = NULL;
        sb->skinned->indices  = NULL;
        skinned_destroy(sb->skinned);
        free(sb->skinned);
    }
    if (sb->skeleton) {
        skeleton_destroy(sb->skeleton);
    }
    free(sb->rest_positions);
    free(sb->bones);
    free(sb->springs);
    free(sb->skin_weights);
    free(sb->vertices);
    free(sb->indices);
    free(sb);
}

void softbody_set_transform(Softbody* sb, vec3 pos, vec3 rot, vec3 scale) {
    if (!sb) return;

    glm_vec3_copy(pos,   sb->pos);
    glm_vec3_copy(rot,   sb->rot);
    glm_vec3_copy(scale, sb->scale);
    rebuild_model_matrix(sb);
}

static inline void vec3_inv_transform(vec3 dst, const vec3 src, const mat4 inv_model) {
    vec4 tmp = { src[0], src[1], src[2], 0.0f };
    vec4 res;
    glm_mat4_mulv(inv_model, tmp, res);
    dst[0] = res[0];
    dst[1] = res[1];
    dst[2] = res[2];
}

static void compute_bone_rotations(Softbody* sb) {
    if (!sb || !sb->bones || !sb->springs) return;
    
    int n = sb->bone_count;
    
    for (int i = 0; i < n; i++) {
        vec3 rest_com = {0,0,0};
        vec3 deformed_com = {0,0,0};
        int neighbor_count = 0;
        
        glm_vec3_add(rest_com, sb->rest_positions[i], rest_com);
        glm_vec3_add(deformed_com, sb->bones[i].position, deformed_com);
        neighbor_count = 1;
        
        for (int s = 0; s < sb->spring_count; s++) {
            SoftbodySpring* spring = &sb->springs[s];
            int neighbor = -1;
            
            if (spring->bone_a == i) neighbor = spring->bone_b;
            else if (spring->bone_b == i) neighbor = spring->bone_a;
            else continue;
            
            glm_vec3_add(rest_com, sb->rest_positions[neighbor], rest_com);
            glm_vec3_add(deformed_com, sb->bones[neighbor].position, deformed_com);
            neighbor_count++;
        }
        
        if (neighbor_count < 3) {
            glm_quat_identity(sb->bones[i].rotation);
            continue;
        }
        
        glm_vec3_scale(rest_com, 1.0f/neighbor_count, rest_com);
        glm_vec3_scale(deformed_com, 1.0f/neighbor_count, deformed_com);
        
        mat3 Apq = GLM_MAT3_ZERO_INIT;
        float weight_sum = 0.0f;
        
        vec3 rest_rel, deformed_rel;
        glm_vec3_sub(sb->rest_positions[i], rest_com, rest_rel);
        glm_vec3_sub(sb->bones[i].position, deformed_com, deformed_rel);
        
        float wi = 1.0f;
        for (int row = 0; row < 3; row++) {
            for (int col = 0; col < 3; col++) {
                Apq[row][col] += wi * rest_rel[row] * deformed_rel[col];
            }
        }
        weight_sum += wi;
        
        for (int s = 0; s < sb->spring_count; s++) {
            SoftbodySpring* spring = &sb->springs[s];
            int neighbor = -1;
            
            if (spring->bone_a == i) neighbor = spring->bone_b;
            else if (spring->bone_b == i) neighbor = spring->bone_a;
            else continue;
            
            glm_vec3_sub(sb->rest_positions[neighbor], rest_com, rest_rel);
            glm_vec3_sub(sb->bones[neighbor].position, deformed_com, deformed_rel);
            
            float w = 1.0f;
            for (int row = 0; row < 3; row++) {
                for (int col = 0; col < 3; col++) {
                    Apq[row][col] += w * rest_rel[row] * deformed_rel[col];
                }
            }
            weight_sum += w;
        }
        
        if (weight_sum > 0.0f) {
            float inv_weight = 1.0f / weight_sum;
            for (int row = 0; row < 3; row++) {
                for (int col = 0; col < 3; col++) {
                    Apq[row][col] *= inv_weight;
                }
            }
        }
        
        mat3 R;
        glm_mat3_copy(Apq, R);
        
        for (int iter = 0; iter < 5; iter++) {
            mat3 R_inv, R_trans;
            float det = glm_mat3_det(R);
            
            if (fabsf(det) < 0.000001f) {
                glm_mat3_identity(R);
                break;
            }
            
            glm_mat3_inv(R, R_inv);
            glm_mat3_transpose_to(R_inv, R_trans);
            
            float diff = 0.0f;
            for (int row = 0; row < 3; row++) {
                for (int col = 0; col < 3; col++) {
                    float new_val = 0.5f * (R[row][col] + R_trans[row][col]);
                    diff += fabsf(new_val - R[row][col]);
                    R[row][col] = new_val;
                }
            }
            
            if (diff < 0.0001f) break;
        }
        
        glm_mat3_quat(R, sb->bones[i].rotation);
        
        float qlen = glm_quat_norm(sb->bones[i].rotation);
        if (qlen < 0.0001f || isnan(qlen)) {
            glm_quat_identity(sb->bones[i].rotation);
        }
    }
}

void softbody_update(Softbody* sb, World* world, float dt) {
    if (!sb || !sb->bones || !sb->springs || !world) return;

    int n          = sb->bone_count;
    float spring_k = sb->config.spring_k;
    float damping  = sb->config.damping;
    float grav     = sb->config.gravity;
    float bounce   = sb->config.bounce_factor;
    float spring_damp = 10.0f;
    float max_scale = fmaxf(sb->scale[0], fmaxf(sb->scale[1], sb->scale[2]));
    float bone_radius = MAX_BONE_RADIUS * max_scale;

    if (dt > 0.02f) dt = 0.02f;
    float sub_dt = dt / (float)SUBSTEPS;

    vec3* world_positions  = (vec3*)malloc((size_t)n * sizeof(vec3));
    vec3* world_velocities = (vec3*)malloc((size_t)n * sizeof(vec3));
    vec3* accelerations    = (vec3*)malloc((size_t)n * sizeof(vec3));
    if (!accelerations || !world_positions || !world_velocities) {
        free(accelerations);
        free(world_positions);
        free(world_velocities);
        return;
    }

    vec3 world_gravity = {0.0f, grav, 0.0f};
    vec3 local_gravity;
    vec3_inv_transform(local_gravity, world_gravity, sb->inv_rot_model);

    vec3 rest_center_world;
    glm_mat4_mulv3(sb->model, sb->rest_center, 1.0f, rest_center_world);

    float rest_avg_radius = 0.0f;
    int moving_count = 0;
    for (int i = 0; i < n; i++) {
        if (sb->bones[i].pinned) continue;
        vec3 rest_world;
        glm_mat4_mulv3(sb->model, sb->rest_positions[i], 1.0f, rest_world);
        rest_avg_radius += glm_vec3_distance(rest_center_world, rest_world);
        moving_count++;
    }
    if (moving_count > 0) rest_avg_radius /= moving_count;

    for (int step = 0; step < SUBSTEPS; step++) {
        for (int i = 0; i < n; i++) {
            if (sb->bones[i].pinned) {
                glm_vec3_copy(sb->rest_positions[i], sb->bones[i].position);
                glm_vec3_zero(sb->bones[i].velocity);
            }
        }

        memset(accelerations, 0, (size_t)n * sizeof(vec3));

        for (int i = 0; i < n; i++) {
            if (sb->bones[i].pinned) continue;
            accelerations[i][0] += local_gravity[0];
            accelerations[i][1] += local_gravity[1];
            accelerations[i][2] += local_gravity[2];
        }

        for (int si = 0; si < sb->spring_count; si++) {
            const SoftbodySpring* s = &sb->springs[si];
            int ba = s->bone_a;
            int bb = s->bone_b;

            if (sb->bones[ba].pinned && sb->bones[bb].pinned) continue;

            vec3 delta;
            glm_vec3_sub(sb->bones[ba].position, sb->bones[bb].position, delta);
            float dist = glm_vec3_norm(delta);
            if (dist < 0.0001f) continue;

            vec3 dir;
            glm_vec3_scale(delta, 1.0f / dist, dir);

            float rest_len = s->rest_length;
            float displacement = dist - rest_len;
            float spring_force_mag = -spring_k * displacement;

            vec3 relVel;
            glm_vec3_sub(sb->bones[ba].velocity, sb->bones[bb].velocity, relVel);
            float velAlongSpring = glm_vec3_dot(relVel, dir);
            float dampForce = -spring_damp * velAlongSpring;

            float total_force = spring_force_mag + dampForce;

            if (!sb->bones[ba].pinned) {
                accelerations[ba][0] += dir[0] * total_force;
                accelerations[ba][1] += dir[1] * total_force;
                accelerations[ba][2] += dir[2] * total_force;
            }
            if (!sb->bones[bb].pinned) {
                accelerations[bb][0] -= dir[0] * total_force;
                accelerations[bb][1] -= dir[1] * total_force;
                accelerations[bb][2] -= dir[2] * total_force;
            }
        }

        for (int i = 0; i < n; i++) {
            if (sb->bones[i].pinned) continue;
            accelerations[i][0] -= damping * sb->bones[i].velocity[0];
            accelerations[i][1] -= damping * sb->bones[i].velocity[1];
            accelerations[i][2] -= damping * sb->bones[i].velocity[2];
        }

        for (int i = 0; i < n; i++) {
            if (sb->bones[i].pinned) continue;

            sb->bones[i].velocity[0] += accelerations[i][0] * sub_dt;
            sb->bones[i].velocity[1] += accelerations[i][1] * sub_dt;
            sb->bones[i].velocity[2] += accelerations[i][2] * sub_dt;

            float speed = sqrtf(sb->bones[i].velocity[0] * sb->bones[i].velocity[0] +
                               sb->bones[i].velocity[1] * sb->bones[i].velocity[1] +
                               sb->bones[i].velocity[2] * sb->bones[i].velocity[2]);
            const float max_speed = 30.0f;
            if (speed > max_speed) {
                float scl = max_speed / speed;
                sb->bones[i].velocity[0] *= scl;
                sb->bones[i].velocity[1] *= scl;
                sb->bones[i].velocity[2] *= scl;
            }

            sb->bones[i].position[0] += sb->bones[i].velocity[0] * sub_dt;
            sb->bones[i].position[1] += sb->bones[i].velocity[1] * sub_dt;
            sb->bones[i].position[2] += sb->bones[i].velocity[2] * sub_dt;
        }

        for (int i = 0; i < n; i++) {
            vec4 lp = { sb->bones[i].position[0], sb->bones[i].position[1],
                        sb->bones[i].position[2], 1.0f };
            vec4 wp;
            glm_mat4_mulv(sb->model, lp, wp);
            world_positions[i][0] = wp[0];
            world_positions[i][1] = wp[1];
            world_positions[i][2] = wp[2];

            vec4 lv = { sb->bones[i].velocity[0], sb->bones[i].velocity[1],
                        sb->bones[i].velocity[2], 0.0f };
            vec4 wv;
            glm_mat4_mulv(sb->model, lv, wv);
            world_velocities[i][0] = wv[0];
            world_velocities[i][1] = wv[1];
            world_velocities[i][2] = wv[2];
        }

        for (int i = 0; i < n; i++) {
            if (sb->bones[i].pinned) continue;
            resolve_collision(world, world_positions[i], world_velocities[i],
                              bone_radius, bounce);
        }

        for (int i = 0; i < n; i++) {
            for (int j = i + 1; j < n; j++) {
                if (sb->bones[i].pinned && sb->bones[j].pinned) continue;

                float dx = world_positions[i][0] - world_positions[j][0];
                float dy = world_positions[i][1] - world_positions[j][1];
                float dz = world_positions[i][2] - world_positions[j][2];
                float dist = sqrtf(dx*dx + dy*dy + dz*dz);
                float min_dist = 2.0f * bone_radius;

                if (dist < min_dist && dist > 0.0001f) {
                    float pen = min_dist - dist;
                    float nx = dx / dist;
                    float ny = dy / dist;
                    float nz = dz / dist;

                    float mass_a = sb->bones[i].pinned ? 1000.0f : 1.0f;
                    float mass_b = sb->bones[j].pinned ? 1000.0f : 1.0f;
                    float total_mass = mass_a + mass_b;

                    float corr_a = pen * (mass_b / total_mass);
                    float corr_b = pen * (mass_a / total_mass);

                    if (!sb->bones[i].pinned) {
                        world_positions[i][0] += nx * corr_a;
                        world_positions[i][1] += ny * corr_a;
                        world_positions[i][2] += nz * corr_a;
                    }
                    if (!sb->bones[j].pinned) {
                        world_positions[j][0] -= nx * corr_b;
                        world_positions[j][1] -= ny * corr_b;
                        world_positions[j][2] -= nz * corr_b;
                    }

                    float relVel_x = world_velocities[i][0] - world_velocities[j][0];
                    float relVel_y = world_velocities[i][1] - world_velocities[j][1];
                    float relVel_z = world_velocities[i][2] - world_velocities[j][2];
                    float relVelNormal = relVel_x*nx + relVel_y*ny + relVel_z*nz;

                    if (relVelNormal < 0) {
                        float restitution = 0.3f;
                        float impulse = -(1.0f + restitution) * relVelNormal / total_mass;

                        if (!sb->bones[i].pinned) {
                            world_velocities[i][0] += nx * impulse * mass_b;
                            world_velocities[i][1] += ny * impulse * mass_b;
                            world_velocities[i][2] += nz * impulse * mass_b;
                        }
                        if (!sb->bones[j].pinned) {
                            world_velocities[j][0] -= nx * impulse * mass_a;
                            world_velocities[j][1] -= ny * impulse * mass_a;
                            world_velocities[j][2] -= nz * impulse * mass_a;
                        }
                    }
                }
            }
        }

        if (moving_count > 0) {
            vec3 world_center = {0,0,0};
            for (int i = 0; i < n; i++) {
                if (sb->bones[i].pinned) continue;
                glm_vec3_add(world_center, world_positions[i], world_center);
            }
            glm_vec3_scale(world_center, 1.0f / moving_count, world_center);

            float current_avg_radius = 0.0f;
            for (int i = 0; i < n; i++) {
                if (sb->bones[i].pinned) continue;
                current_avg_radius += glm_vec3_distance(world_center, world_positions[i]);
            }
            current_avg_radius /= moving_count;

            if (current_avg_radius > 0.0001f && rest_avg_radius > 0.0001f) {
                float ratio = rest_avg_radius / current_avg_radius;
                float strength = 0.15f;
                float target_ratio = 1.0f + (ratio - 1.0f) * strength;
                if (target_ratio > 1.0f) {
                    for (int i = 0; i < n; i++) {
                        if (sb->bones[i].pinned) continue;
                        vec3 dir;
                        glm_vec3_sub(world_positions[i], world_center, dir);
                        float len = glm_vec3_norm(dir);
                        if (len > 0.0001f) {
                            glm_vec3_scale(dir, 1.0f / len, dir);
                            float new_len = len * target_ratio;
                            world_positions[i][0] = world_center[0] + dir[0] * new_len;
                            world_positions[i][1] = world_center[1] + dir[1] * new_len;
                            world_positions[i][2] = world_center[2] + dir[2] * new_len;
                        }
                    }
                }
            }
        }

        for (int i = 0; i < n; i++) {
            vec4 wp = { world_positions[i][0], world_positions[i][1],
                        world_positions[i][2], 1.0f };
            vec4 lp;
            glm_mat4_mulv(sb->inv_model, wp, lp);
            sb->bones[i].position[0] = lp[0];
            sb->bones[i].position[1] = lp[1];
            sb->bones[i].position[2] = lp[2];

            vec3_inv_transform(sb->bones[i].velocity, world_velocities[i], sb->inv_model);

            if (isnan(sb->bones[i].position[0]) || isnan(sb->bones[i].position[1]) || isnan(sb->bones[i].position[2])) {
                glm_vec3_copy(sb->rest_positions[i], sb->bones[i].position);
                glm_vec3_zero(sb->bones[i].velocity);
            }
        }
    }

    free(accelerations);
    free(world_positions);
    free(world_velocities);
}

Skinned_render_request* softbody_render_request(Softbody* sb) {
    if (!sb || !sb->skinned) return NULL;
    Skinned_render_request* req =
        (Skinned_render_request*)calloc(1, sizeof(Skinned_render_request));
    if (!req) return NULL;
    req->skinned  = sb->skinned;
    req->skeleton = sb->skeleton;
    req->anim     = NULL;
    req->look.enabled = 0;
    return req;
}

static void softbody_update_normals(Softbody* sb) {
    if (!sb || !sb->skinned || sb->vertex_count < 3 || sb->index_count < 3) return;
    if (!sb->bones || !sb->rest_positions) return;

    int vc = sb->vertex_count;
    int ic = sb->index_count;
    const float* verts = sb->vertices;

    // Allocate arrays
    vec3* bind_positions = (vec3*)malloc((size_t)vc * sizeof(vec3));
    vec3* deformed = (vec3*)malloc((size_t)vc * sizeof(vec3));
    if (!bind_positions || !deformed) {
        free(bind_positions);
        free(deformed);
        return;
    }

    // Compute deformed positions WITH bone rotation
    for (int i = 0; i < vc; i++) {
        vec3 bind_pos = { verts[i*16+0], verts[i*16+1], verts[i*16+2] };
        
        // Store bind-pose position
        bind_positions[i][0] = bind_pos[0];
        bind_positions[i][1] = bind_pos[1];
        bind_positions[i][2] = bind_pos[2];

        const float* ids_f = &verts[i*16+8];
        const float* w     = &verts[i*16+12];

        // Compute weighted average of bone transformations
        vec3 dpos = { 0.0f, 0.0f, 0.0f };
        float wsum = w[0] + w[1] + w[2] + w[3];
        
        if (wsum >= 0.0001f) {
            for (int j = 0; j < 4; j++) {
                int bj = (int)ids_f[j];
                if (bj >= 0 && bj < sb->bone_count && w[j] > 0.0f) {
                    float weight = w[j] / wsum;
                    
                    // Get the vertex offset from bone in rest pose
                    vec3 rest_offset;
                    glm_vec3_sub(bind_pos, sb->rest_positions[bj], rest_offset);
                    
                    // Rotate the offset by the bone's current rotation
                    vec3 rotated_offset;
                    glm_quat_rotatev(sb->bones[bj].rotation, rest_offset, rotated_offset);
                    
                    // The new position is bone position + rotated offset
                    vec3 new_pos;
                    glm_vec3_add(sb->bones[bj].position, rotated_offset, new_pos);
                    
                    // Accumulate weighted position
                    dpos[0] += weight * new_pos[0];
                    dpos[1] += weight * new_pos[1];
                    dpos[2] += weight * new_pos[2];
                }
            }
            
            deformed[i][0] = dpos[0];
            deformed[i][1] = dpos[1];
            deformed[i][2] = dpos[2];
        } else {
            // No bone influence, keep original position
            deformed[i][0] = bind_pos[0];
            deformed[i][1] = bind_pos[1];
            deformed[i][2] = bind_pos[2];
        }
    }

    // Initialize normals array to zero
    vec3* normals = (vec3*)calloc((size_t)vc, sizeof(vec3));
    if (!normals) {
        free(bind_positions);
        free(deformed);
        return;
    }

    // Compute face normals with orientation check
    for (int k = 0; k < ic; k += 3) {
        int i0 = sb->indices[k];
        int i1 = sb->indices[k+1];
        int i2 = sb->indices[k+2];

        // Compute original face normal from bind-pose positions
        vec3 o1, o2, N_orig;
        glm_vec3_sub(bind_positions[i1], bind_positions[i0], o1);
        glm_vec3_sub(bind_positions[i2], bind_positions[i0], o2);
        glm_vec3_cross(o1, o2, N_orig);

        // Compute deformed face normal
        vec3 e1, e2, N;
        glm_vec3_sub(deformed[i1], deformed[i0], e1);
        glm_vec3_sub(deformed[i2], deformed[i0], e2);
        glm_vec3_cross(e1, e2, N);

        // If the deformed normal points opposite to the original, flip it
        if (glm_vec3_dot(N, N_orig) < 0.0f) {
            glm_vec3_negate(N);
        }

        // Accumulate the corrected normal to all three vertices
        glm_vec3_add(normals[i0], N, normals[i0]);
        glm_vec3_add(normals[i1], N, normals[i1]);
        glm_vec3_add(normals[i2], N, normals[i2]);
    }

    free(bind_positions);
    free(deformed);

    // Copy vertex data and update normals in GPU buffer
    float* gpu_data = (float*)malloc((size_t)vc * 16 * sizeof(float));
    if (!gpu_data) {
        free(normals);
        return;
    }
    memcpy(gpu_data, verts, (size_t)vc * 16 * sizeof(float));

    // Normalize the accumulated normals and store them
    for (int i = 0; i < vc; i++) {
        float len = glm_vec3_norm(normals[i]);
        if (len > 0.0001f) {
            gpu_data[i*16+5] = normals[i][0] / len;
            gpu_data[i*16+6] = normals[i][1] / len;
            gpu_data[i*16+7] = normals[i][2] / len;
        }
        // If length is near zero, keep original normals from verts
    }
    free(normals);

    // Upload updated vertex data to GPU
    glBindBuffer(GL_ARRAY_BUFFER, sb->skinned->gpu.vbo.id);
    glBufferSubData(GL_ARRAY_BUFFER, 0, (size_t)vc * 16 * sizeof(float), gpu_data);
    free(gpu_data);
}

void softbody_render(Softbody* sb, Program* prog) {
    if (!sb || !sb->skeleton || !sb->skinned || !prog) return;
    if (sb->index_count < 3) return;

    compute_bone_rotations(sb);

    for (int i = 0; i < sb->bone_count; i++) {
        mat4 offset;
        mat4 rot_matrix;
        
        glm_quat_mat4(sb->bones[i].rotation, rot_matrix);
        
        glm_mat4_identity(offset);
        glm_translate(offset, sb->bones[i].position);
        glm_mat4_mul(offset, rot_matrix, offset);
        
        skeleton_set_bone_offset(sb->skeleton, i, offset);
    }

    softbody_update_normals(sb);

    glm_vec3_copy(sb->pos,   sb->skinned->pos);
    glm_vec3_copy(sb->rot,   sb->skinned->rot);
    glm_vec3_copy(sb->scale, sb->skinned->scale);

    skinned_render(sb->skinned, prog, 0.0f, NULL);
}