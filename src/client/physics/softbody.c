#include "physics/softbody.h"
#include "core/gfx.h"
#include "core/skin.h"
#include "utils/mesh_raw.h"
#include "utils/obj.h"
#include "utils/rand.h"
#include "core/world.h"
#include "core/tile.h"

#include <cglm/cglm.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>

#ifdef SOFTBODY_USE_GLTF
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#endif

#define SUBSTEPS 32
#define MAX_BONE_RADIUS 0.05f

typedef struct {
    vec3 position;
    vec3 velocity;
    int   pinned;
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
    
    for (int cx = min_x; cx <= max_x; cx++) {
        for (int cy = min_y; cy <= max_y; cy++) {
            for (int cz = min_z; cz <= max_z; cz++) {
                if (!is_solid_block(world, cx, cy, cz)) continue;
                
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
                    
                    vel[0] *= 0.95f;
                    vel[1] *= 0.95f;
                    vel[2] *= 0.95f;
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

typedef struct {
    int a, b, c, d;
} Tetra;

static float orient3d_det(const vec3 a, const vec3 b, const vec3 c, const vec3 d) {
    float adx = a[0] - d[0], ady = a[1] - d[1], adz = a[2] - d[2];
    float bdx = b[0] - d[0], bdy = b[1] - d[1], bdz = b[2] - d[2];
    float cdx = c[0] - d[0], cdy = c[1] - d[1], cdz = c[2] - d[2];
    return adx * (bdy * cdz - bdz * cdy)
         + ady * (bdz * cdx - bdx * cdz)
         + adz * (bdx * cdy - bdy * cdx);
}

static int circumsphere_contains(const vec3 a, const vec3 b, const vec3 c, const vec3 d, const vec3 p) {
    float ax = a[0] - p[0], ay = a[1] - p[1], az = a[2] - p[2];
    float bx = b[0] - p[0], by = b[1] - p[1], bz = b[2] - p[2];
    float cx = c[0] - p[0], cy = c[1] - p[1], cz = c[2] - p[2];
    float dx = d[0] - p[0], dy = d[1] - p[1], dz = d[2] - p[2];
    float len_a = ax*ax + ay*ay + az*az;
    float len_b = bx*bx + by*by + bz*bz;
    float len_c = cx*cx + cy*cy + cz*cz;
    float len_d = dx*dx + dy*dy + dz*dz;
    float det = ax * (by * (cz*len_d - dz*len_c) + cy * (dz*len_b - bz*len_d) + dy * (bz*len_c - cz*len_b))
              - ay * (bx * (cz*len_d - dz*len_c) + cx * (dz*len_b - bz*len_d) + dx * (bz*len_c - cz*len_b))
              + az * (bx * (cy*len_d - dy*len_c) + cx * (dy*len_b - by*len_d) + dx * (by*len_c - cy*len_b))
              - len_a * (bx * (cy*dz - dy*cz) + cx * (dy*bz - by*dz) + dx * (by*cz - cy*bz));
    return det > 0.0f;
}

typedef struct {
    int v[3];
} Face;

static int face_eq(Face f1, Face f2) {
    int sorted1[3] = {f1.v[0], f1.v[1], f1.v[2]};
    int sorted2[3] = {f2.v[0], f2.v[1], f2.v[2]};
    for (int i = 0; i < 3; i++) {
        for (int j = i+1; j < 3; j++) {
            if (sorted1[i] > sorted1[j]) { int t = sorted1[i]; sorted1[i] = sorted1[j]; sorted1[j] = t; }
            if (sorted2[i] > sorted2[j]) { int t = sorted2[i]; sorted2[i] = sorted2[j]; sorted2[j] = t; }
        }
    }
    return sorted1[0]==sorted2[0] && sorted1[1]==sorted2[1] && sorted1[2]==sorted2[2];
}

static void delaunay_tetrahedralize(const vec3* points, int num_points,
                                     Tetra** out_tets, int* out_tet_count) {
    *out_tets = NULL;
    *out_tet_count = 0;
    if (num_points < 4) return;

    vec3 bmin, bmax;
    glm_vec3_copy(points[0], bmin);
    glm_vec3_copy(points[0], bmax);
    for (int i = 1; i < num_points; i++) {
        if (points[i][0] < bmin[0]) bmin[0] = points[i][0];
        if (points[i][1] < bmin[1]) bmin[1] = points[i][1];
        if (points[i][2] < bmin[2]) bmin[2] = points[i][2];
        if (points[i][0] > bmax[0]) bmax[0] = points[i][0];
        if (points[i][1] > bmax[1]) bmax[1] = points[i][1];
        if (points[i][2] > bmax[2]) bmax[2] = points[i][2];
    }
    float dx = bmax[0] - bmin[0], dy = bmax[1] - bmin[1], dz = bmax[2] - bmin[2];
    float max_extent = dx;
    if (dy > max_extent) max_extent = dy;
    if (dz > max_extent) max_extent = dz;
    if (max_extent < 0.001f) max_extent = 1.0f;
    float off = max_extent * 10.0f;

    vec3 super[4];
    glm_vec3_copy((vec3){bmin[0]-off, bmin[1]-off, bmin[2]-off}, super[0]);
    glm_vec3_copy((vec3){bmax[0]+off, bmin[1]-off, bmin[2]-off}, super[1]);
    glm_vec3_copy((vec3){bmin[0]-off, bmax[1]+off, bmin[2]-off}, super[2]);
    glm_vec3_copy((vec3){bmin[0]-off, bmin[1]-off, bmax[2]+off}, super[3]);

    int tet_cap = 64;
    int tet_count = 1;
    Tetra* tets = malloc(tet_cap * sizeof(Tetra));
    tets[0] = (Tetra){0, 1, 2, 3};

    vec3* all_pts = malloc((num_points + 4) * sizeof(vec3));
    memcpy(all_pts, super, 4 * sizeof(vec3));
    memcpy(all_pts + 4, points, num_points * sizeof(vec3));

    for (int pi = 4; pi < num_points + 4; pi++) {
        int bad_cap = 64, bad_count = 0;
        int* bad = malloc(bad_cap * sizeof(int));
        for (int ti = 0; ti < tet_count; ti++) {
            Tetra t = tets[ti];
            if (circumsphere_contains(all_pts[t.a], all_pts[t.b], all_pts[t.c], all_pts[t.d], all_pts[pi])) {
                if (bad_count >= bad_cap) { bad_cap *= 2; bad = realloc(bad, bad_cap * sizeof(int)); }
                bad[bad_count++] = ti;
            }
        }

        Face* faces = NULL;
        int face_cap = 0, face_count = 0;
        for (int bi = 0; bi < bad_count; bi++) {
            Tetra t = tets[bad[bi]];
            int fv[4][3] = {{t.a,t.b,t.c}, {t.a,t.b,t.d}, {t.a,t.c,t.d}, {t.b,t.c,t.d}};
            for (int f = 0; f < 4; f++) {
                Face ft;
                ft.v[0] = fv[f][0]; ft.v[1] = fv[f][1]; ft.v[2] = fv[f][2];
                int found = 0;
                for (int fi = 0; fi < face_count; fi++) {
                    if (face_eq(faces[fi], ft)) {
                        for (int k = fi; k < face_count-1; k++) faces[k] = faces[k+1];
                        face_count--;
                        found = 1;
                        break;
                    }
                }
                if (!found) {
                    if (face_count >= face_cap) {
                        face_cap = face_cap ? face_cap*2 : 64;
                        faces = realloc(faces, face_cap * sizeof(Face));
                    }
                    faces[face_count++] = ft;
                }
            }
        }

        int new_tet_cap = tet_count + face_count;
        Tetra* new_tets = malloc(new_tet_cap * sizeof(Tetra));
        int new_count = 0;
        for (int ti = 0; ti < tet_count; ti++) {
            int bad_flag = 0;
            for (int bi = 0; bi < bad_count; bi++) if (ti == bad[bi]) { bad_flag = 1; break; }
            if (!bad_flag) new_tets[new_count++] = tets[ti];
        }
        for (int fi = 0; fi < face_count; fi++) {
            Face ft = faces[fi];
            if (orient3d_det(all_pts[ft.v[0]], all_pts[ft.v[1]], all_pts[ft.v[2]], all_pts[pi]) > 0) {
                new_tets[new_count++] = (Tetra){ft.v[0], ft.v[1], ft.v[2], pi};
            } else {
                new_tets[new_count++] = (Tetra){ft.v[0], ft.v[2], ft.v[1], pi};
            }
        }

        free(tets);
        tets = new_tets;
        tet_count = new_count;
        free(bad);
        free(faces);
    }

    int final_cap = tet_count;
    Tetra* final_tets = malloc(final_cap * sizeof(Tetra));
    int final_count = 0;
    for (int ti = 0; ti < tet_count; ti++) {
        Tetra t = tets[ti];
        if (t.a >= 4 && t.b >= 4 && t.c >= 4 && t.d >= 4) {
            final_tets[final_count++] = (Tetra){t.a-4, t.b-4, t.c-4, t.d-4};
        }
    }
    free(tets);
    free(all_pts);

    *out_tets = final_tets;
    *out_tet_count = final_count;
}

static void build_nearest_neighbour_springs(Softbody* sb) {
    int n = sb->bone_count;
    if (n < 2) return;
    int k = n < 5 ? n-1 : 4;
    for (int i = 0; i < n; i++) {
        typedef struct { int idx; float dist; } Neighbour;
        Neighbour neigh[256];
        int nn = 0;
        for (int j = 0; j < n; j++) {
            if (i == j) continue;
            float d = sqrtf(vec3_dist_sq(sb->rest_positions[i], sb->rest_positions[j]));
            neigh[nn].idx = j;
            neigh[nn].dist = d;
            nn++;
        }
        for (int a = 0; a < nn-1; a++) {
            int best = a;
            for (int b = a+1; b < nn; b++)
                if (neigh[b].dist < neigh[best].dist) best = b;
            Neighbour tmp = neigh[a];
            neigh[a] = neigh[best];
            neigh[best] = tmp;
        }
        int add = k < nn ? k : nn;
        for (int a = 0; a < add; a++) {
            int j = neigh[a].idx;
            int exists = 0;
            for (int s = 0; s < sb->spring_count; s++) {
                if ((sb->springs[s].bone_a == i && sb->springs[s].bone_b == j) ||
                    (sb->springs[s].bone_a == j && sb->springs[s].bone_b == i)) {
                    exists = 1; break;
                }
            }
            if (!exists) {
                add_spring(&sb->springs, &sb->spring_count, i, j, neigh[a].dist);
            }
        }
    }
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
    if (conf.bone_count <= 0) conf.bone_count = 16;
    if (conf.spring_k <= 0.0f) conf.spring_k = 30.0f;
    if (conf.damping <= 0.0f)  conf.damping = 5.0f;
    if (conf.gravity == 0.0f)  conf.gravity = -9.81f;
    if (conf.bounce_factor <= 0.0f) conf.bounce_factor = 0.15f;

    int target_bones = conf.bone_count;
    if (target_bones > raw_vcount) target_bones = raw_vcount;

    Softbody* sb = (Softbody*)calloc(1, sizeof(Softbody));
    if (!sb) { free(raw_verts); free(raw_indices); return NULL; }

    sb->pos[0] = sb->pos[1] = sb->pos[2] = 0.0f;
    sb->rot[0] = sb->rot[1] = sb->rot[2] = 0.0f;
    sb->scale[0] = sb->scale[1] = sb->scale[2] = 1.0f;
    memcpy(&sb->config, &conf, sizeof(conf));

    vec3* centroids = calloc(target_bones, sizeof(vec3));
    int first = (int)(RANDF() * raw_vcount);
    if (first >= raw_vcount) first = raw_vcount - 1;
    glm_vec3_copy((vec3){raw_verts[first*8], raw_verts[first*8+1], raw_verts[first*8+2]}, centroids[0]);

    float* d2 = malloc(raw_vcount * sizeof(float));
    for (int j = 1; j < target_bones; j++) {
        float sum = 0.0f;
        for (int i = 0; i < raw_vcount; i++) {
            float min_d = FLT_MAX;
            vec3 vp = {raw_verts[i*8], raw_verts[i*8+1], raw_verts[i*8+2]};
            for (int k = 0; k < j; k++) {
                float dd = vec3_dist_sq(vp, centroids[k]);
                if (dd < min_d) min_d = dd;
            }
            d2[i] = min_d;
            sum += min_d;
        }
        float r = RANDF() * sum;
        int idx = 0;
        float acc = 0.0f;
        for (; idx < raw_vcount; idx++) {
            acc += d2[idx];
            if (acc >= r) break;
        }
        if (idx == raw_vcount) idx = raw_vcount-1;
        glm_vec3_copy((vec3){raw_verts[idx*8], raw_verts[idx*8+1], raw_verts[idx*8+2]}, centroids[j]);
    }
    free(d2);

    int* closest = malloc(raw_vcount * sizeof(int));
    for (int iter = 0; iter < 10; iter++) {
        for (int i = 0; i < raw_vcount; i++) {
            float best = FLT_MAX;
            int best_c = 0;
            vec3 vp = {raw_verts[i*8], raw_verts[i*8+1], raw_verts[i*8+2]};
            for (int c = 0; c < target_bones; c++) {
                float d = vec3_dist_sq(vp, centroids[c]);
                if (d < best) { best = d; best_c = c; }
            }
            closest[i] = best_c;
        }
        for (int c = 0; c < target_bones; c++) {
            vec3 sum = {0,0,0};
            int cnt = 0;
            for (int i = 0; i < raw_vcount; i++) {
                if (closest[i] == c) {
                    sum[0] += raw_verts[i*8+0]; sum[1] += raw_verts[i*8+1]; sum[2] += raw_verts[i*8+2];
                    cnt++;
                }
            }
            if (cnt > 0) {
                sum[0] /= cnt; sum[1] /= cnt; sum[2] /= cnt;
                glm_vec3_copy(sum, centroids[c]);
            }
        }
    }
    free(closest);

    sb->bone_count = target_bones;
    sb->rest_positions = calloc(target_bones, sizeof(vec3));
    sb->bones = calloc(target_bones, sizeof(SoftbodyBoneNode));
    if (!sb->rest_positions || !sb->bones) {
        free(centroids);
        softbody_destroy(sb);
        free(raw_verts);
        free(raw_indices);
        return NULL;
    }

    for (int i = 0; i < target_bones; i++) {
        glm_vec3_copy(centroids[i], sb->rest_positions[i]);
        glm_vec3_copy(centroids[i], sb->bones[i].position);
        glm_vec3_zero(sb->bones[i].velocity);
        sb->bones[i].pinned = 0;
    }
    free(centroids);

    float* skin_w = calloc(raw_vcount, 8 * sizeof(float));
    if (!skin_w) {
        softbody_destroy(sb);
        free(raw_verts);
        free(raw_indices);
        return NULL;
    }
    compute_skin_weights(raw_verts, raw_vcount, sb->rest_positions, sb->bone_count, skin_w);
    sb->skin_weights = skin_w;

    sb->springs = NULL;
    sb->spring_count = 0;

    Tetra* tets = NULL;
    int tet_count = 0;
    delaunay_tetrahedralize(sb->rest_positions, sb->bone_count, &tets, &tet_count);
    int spring_from_delaunay = 0;
    if (tets && tet_count > 0) {
        for (int t = 0; t < tet_count; t++) {
            int ids[4] = {tets[t].a, tets[t].b, tets[t].c, tets[t].d};
            for (int i = 0; i < 4; i++) {
                for (int j = i+1; j < 4; j++) {
                    int ba = ids[i], bb = ids[j];
                    int already = 0;
                    for (int s = 0; s < sb->spring_count; s++) {
                        if ((sb->springs[s].bone_a == ba && sb->springs[s].bone_b == bb) ||
                            (sb->springs[s].bone_a == bb && sb->springs[s].bone_b == ba)) {
                            already = 1; break;
                        }
                    }
                    if (!already) {
                        float rest = sqrtf(vec3_dist_sq(sb->rest_positions[ba], sb->rest_positions[bb]));
                        add_spring(&sb->springs, &sb->spring_count, ba, bb, rest);
                        spring_from_delaunay++;
                    }
                }
            }
        }
        free(tets);
    }

    if (spring_from_delaunay < sb->bone_count * 2) {
        build_nearest_neighbour_springs(sb);
    }

    sb->grid_x = sb->bone_count;
    sb->grid_y = 1;
    sb->grid_z = 1;

    sb->vertex_count = 0;
    sb->vertices = build_skinned_vertices(raw_verts, raw_vcount, skin_w, &sb->vertex_count);
    sb->indices = raw_indices;
    sb->index_count = raw_icount;
    free(raw_verts);

    sb->skeleton = softbody_build_skeleton(sb->rest_positions, sb->bone_count);
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

void softbody_update(Softbody* sb, World* world, float dt) {
    if (!sb || !sb->bones || !sb->springs || !world) return;

    int n          = sb->bone_count;
    float spring_k = sb->config.spring_k;
    float damping  = sb->config.damping;
    float grav     = sb->config.gravity;
    float bounce   = sb->config.bounce_factor;
    float spring_damp = 2.0f;
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

    for (int step = 0; step < SUBSTEPS; step++) {
        memset(accelerations, 0, (size_t)n * sizeof(vec3));

        vec4 world_grav = { 0.0f, grav, 0.0f, 0.0f };
        vec4 local_grav;
        glm_mat4_mulv(sb->inv_rot_model, world_grav, local_grav);

        for (int i = 0; i < n; i++) {
            if (sb->bones[i].pinned) continue;
            accelerations[i][0] += local_grav[0];
            accelerations[i][1] += local_grav[1];
            accelerations[i][2] += local_grav[2];
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

        for (int i = 0; i < n; i++) {
            vec4 wp = { world_positions[i][0], world_positions[i][1],
                        world_positions[i][2], 1.0f };
            vec4 lp;
            glm_mat4_mulv(sb->inv_model, wp, lp);
            sb->bones[i].position[0] = lp[0];
            sb->bones[i].position[1] = lp[1];
            sb->bones[i].position[2] = lp[2];

            vec3_inv_transform(sb->bones[i].velocity, world_velocities[i], sb->inv_model);
        }

        for (int i = 0; i < n; i++) {
            if (isnan(sb->bones[i].position[0]) || isnan(sb->bones[i].position[1]) || isnan(sb->bones[i].position[2])) {
                for (int j = 0; j < n; j++) {
                    glm_vec3_copy(sb->rest_positions[j], sb->bones[j].position);
                    glm_vec3_zero(sb->bones[j].velocity);
                }
                break;
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

    vec3* deformed = (vec3*)malloc((size_t)vc * sizeof(vec3));
    if (!deformed) return;

    for (int i = 0; i < vc; i++) {
        vec3 bind_pos = { verts[i*16+0], verts[i*16+1], verts[i*16+2] };
        vec3 dpos = { 0.0f, 0.0f, 0.0f };

        const float* ids_f = &verts[i*16+8];
        const float* w     = &verts[i*16+12];

        float wsum = w[0] + w[1] + w[2] + w[3];
        if (wsum >= 0.0001f) {
            for (int j = 0; j < 4; j++) {
                int bj = (int)ids_f[j];
                if (bj >= 0 && bj < sb->bone_count && w[j] > 0.0f) {
                    float weight = w[j] / wsum;
                    dpos[0] += weight * (sb->bones[bj].position[0] - sb->rest_positions[bj][0]);
                    dpos[1] += weight * (sb->bones[bj].position[1] - sb->rest_positions[bj][1]);
                    dpos[2] += weight * (sb->bones[bj].position[2] - sb->rest_positions[bj][2]);
                }
            }
        }

        deformed[i][0] = bind_pos[0] + dpos[0];
        deformed[i][1] = bind_pos[1] + dpos[1];
        deformed[i][2] = bind_pos[2] + dpos[2];
    }

    vec3* normals = (vec3*)calloc((size_t)vc, sizeof(vec3));
    if (!normals) { free(deformed); return; }

    for (int k = 0; k < ic; k += 3) {
        int i0 = sb->indices[k];
        int i1 = sb->indices[k+1];
        int i2 = sb->indices[k+2];

        vec3 e1, e2, N;
        glm_vec3_sub(deformed[i1], deformed[i0], e1);
        glm_vec3_sub(deformed[i2], deformed[i0], e2);
        glm_vec3_cross(e1, e2, N);

        glm_vec3_add(normals[i0], N, normals[i0]);
        glm_vec3_add(normals[i1], N, normals[i1]);
        glm_vec3_add(normals[i2], N, normals[i2]);
    }
    free(deformed);

    float* gpu_data = (float*)malloc((size_t)vc * 16 * sizeof(float));
    if (!gpu_data) { free(normals); return; }
    memcpy(gpu_data, verts, (size_t)vc * 16 * sizeof(float));

    for (int i = 0; i < vc; i++) {
        float len = glm_vec3_norm(normals[i]);
        if (len > 0.0001f) {
            gpu_data[i*16+5] = normals[i][0] / len;
            gpu_data[i*16+6] = normals[i][1] / len;
            gpu_data[i*16+7] = normals[i][2] / len;
        }
    }
    free(normals);

    glBindBuffer(GL_ARRAY_BUFFER, sb->skinned->gpu.vbo.id);
    glBufferSubData(GL_ARRAY_BUFFER, 0, (size_t)vc * 16 * sizeof(float), gpu_data);
    free(gpu_data);
}

void softbody_render(Softbody* sb, Program* prog) {
    if (!sb || !sb->skeleton || !sb->skinned || !prog) return;
    if (sb->index_count < 3) return;

    for (int i = 0; i < sb->bone_count; i++) {
        mat4 offset;
        glm_mat4_identity(offset);
        glm_translate(offset, sb->bones[i].position);
        skeleton_set_bone_offset(sb->skeleton, i, offset);
    }

    softbody_update_normals(sb);

    glm_vec3_copy(sb->pos,   sb->skinned->pos);
    glm_vec3_copy(sb->rot,   sb->skinned->rot);
    glm_vec3_copy(sb->scale, sb->skinned->scale);

    skinned_render(sb->skinned, prog, 0.0f, NULL);
}