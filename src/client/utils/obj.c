#include "utils/obj.h"
#include "utils/fm.h"
#include "utils/log.h"
#include "core/gfx.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

typedef struct {
    int v;
    int vt;
    int vn;
} ObjIndex;

typedef struct {
    ObjIndex* keys;
    int* values;
    unsigned char* used;
    int cap;
    int count;
} ObjMap;

static void* xmalloc(size_t n) {
    void* p = malloc(n);
    ASSERT(p != NULL, "malloc");
    return p;
}

static void* xcalloc(size_t n, size_t sz) {
    void* p = calloc(n, sz);
    ASSERT(p != NULL, "calloc");
    return p;
}

static void* xrealloc(void* p, size_t n) {
    void* q = realloc(p, n);
    ASSERT(q != NULL, "realloc");
    return q;
}

static int is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
}

static void skip_ws(const char** s) {
    while (**s && is_ws(**s)) (*s)++;
}

static int parse_float_field(const char** s, float* out) {
    skip_ws(s);
    if (!**s) return 0;
    char* end = NULL;
    float v = strtof(*s, &end);
    if (end == *s) return 0;
    *out = v;
    *s = end;
    return 1;
}

static int parse_int_cstr(const char* s, int* out) {
    if (!s || !*s) return 0;
    char* end = NULL;
    long v = strtol(s, &end, 10);
    if (end == s || *end != '\0') return 0;
    *out = (int)v;
    return 1;
}

static int parse_face_token_range(const char* start, const char* end, ObjIndex* out) {
    size_t len = (size_t)(end - start);
    if (len == 0) return 0;

    char* tmp = (char*)malloc(len + 1);
    memcpy(tmp, start, len);
    tmp[len] = '\0';

    int v = 0, vt = 0, vn = 0;

    char* s1 = strchr(tmp, '/');
    if (!s1) {
        if (!parse_int_cstr(tmp, &v)) {
            free(tmp);
            return 0;
        }
    } else {
        *s1 = '\0';
        if (!parse_int_cstr(tmp, &v)) {
            free(tmp);
            return 0;
        }

        char* s2 = s1 + 1;
        char* s3 = strchr(s2, '/');
        if (!s3) {
            if (*s2 && !parse_int_cstr(s2, &vt)) {
                free(tmp);
                return 0;
            }
        } else {
            *s3 = '\0';
            if (*s2 && !parse_int_cstr(s2, &vt)) {
                free(tmp);
                return 0;
            }
            char* s4 = s3 + 1;
            if (*s4 && !parse_int_cstr(s4, &vn)) {
                free(tmp);
                return 0;
            }
        }
    }

    free(tmp);

    out->v = v;
    out->vt = vt;
    out->vn = vn;
    return 1;
}

static int resolve_index(int idx, int count) {
    if (idx > 0) return idx - 1;
    if (idx < 0) return count + idx;
    return -1;
}

static uint64_t hash_obj_index(ObjIndex k) {
    uint64_t h = 1469598103934665603ULL;
    uint32_t a = (uint32_t)(k.v + 1);
    uint32_t b = (uint32_t)(k.vt + 2);
    uint32_t c = (uint32_t)(k.vn + 3);

    for (int i = 0; i < 4; i++) {
        h ^= (uint64_t)((a >> (i * 8)) & 0xFFu);
        h *= 1099511628211ULL;
    }
    for (int i = 0; i < 4; i++) {
        h ^= (uint64_t)((b >> (i * 8)) & 0xFFu);
        h *= 1099511628211ULL;
    }
    for (int i = 0; i < 4; i++) {
        h ^= (uint64_t)((c >> (i * 8)) & 0xFFu);
        h *= 1099511628211ULL;
    }
    return h;
}

static void map_init(ObjMap* m) {
    m->keys = NULL;
    m->values = NULL;
    m->used = NULL;
    m->cap = 0;
    m->count = 0;
}

static void map_free(ObjMap* m) {
    free(m->keys);
    free(m->values);
    free(m->used);
    m->keys = NULL;
    m->values = NULL;
    m->used = NULL;
    m->cap = 0;
    m->count = 0;
}

static void map_rehash(ObjMap* m, int new_cap) {
    ObjIndex* old_keys = m->keys;
    int* old_values = m->values;
    unsigned char* old_used = m->used;
    int old_cap = m->cap;

    m->keys = (ObjIndex*)calloc(new_cap, sizeof(ObjIndex));
    m->values = (int*)calloc(new_cap, sizeof(int));
    m->used = (unsigned char*)calloc(new_cap, sizeof(unsigned char));
    m->cap = new_cap;
    m->count = 0;

    for (int i = 0; i < old_cap; i++) {
        if (!old_used[i]) continue;

        ObjIndex key = old_keys[i];
        int value = old_values[i];
        uint64_t h = hash_obj_index(key);
        int mask = new_cap - 1;
        int pos = (int)(h & (uint64_t)mask);

        while (m->used[pos]) pos = (pos + 1) & mask;

        m->used[pos] = 1;
        m->keys[pos] = key;
        m->values[pos] = value;
        m->count++;
    }

    free(old_keys);
    free(old_values);
    free(old_used);
}

static int map_get_or_add(ObjMap* m, ObjIndex key, int value_if_new, int* is_new) {
    if (m->cap == 0) map_rehash(m, 256);
    else if ((m->count + 1) * 10 >= m->cap * 7) map_rehash(m, m->cap * 2);

    uint64_t h = hash_obj_index(key);
    int mask = m->cap - 1;
    int pos = (int)(h & (uint64_t)mask);

    while (m->used[pos]) {
        ObjIndex k = m->keys[pos];
        if (k.v == key.v && k.vt == key.vt && k.vn == key.vn) {
            if (is_new) *is_new = 0;
            return m->values[pos];
        }
        pos = (pos + 1) & mask;
    }

    m->used[pos] = 1;
    m->keys[pos] = key;
    m->values[pos] = value_if_new;
    m->count++;

    if (is_new) *is_new = 1;
    return value_if_new;
}

Render_request* obj_load_render_request(const char* path) {
    ASSERT(path != NULL, "path is null");

    File f = file_open(path);
    ASSERT(f.data != NULL, "file open failed");

    float* pos = NULL;
    float* nrm = NULL;
    float* tex = NULL;
    int pos_count = 0, nrm_count = 0, tex_count = 0;
    int pos_cap = 0, nrm_cap = 0, tex_cap = 0;

    int* indices = NULL;
    int indices_count = 0;
    int indices_cap = 0;

    float* vertices = NULL;
    int vertices_count = 0;
    int vertices_cap = 0;

    ObjMap map;
    map_init(&map);

    ObjIndex* face_tmp = (ObjIndex*)malloc(16 * sizeof(ObjIndex));
    int face_cap = 16;

    const char* s = f.data;
    while (*s) {
        while (*s == '\r' || *s == '\n') s++;
        const char* line = s;
        while (*s && *s != '\n' && *s != '\r') s++;
        const char* end = s;

        if (*s == '\r' && s[1] == '\n') s += 2;
        else if (*s) s++;

        while (line < end && is_ws(*line)) line++;
        if (line >= end || *line == '#') continue;

        if (line[0] == 'v' && is_ws(line[1])) {
            const char* p = line + 1;
            float x, y, z;
            if (!parse_float_field(&p, &x)) continue;
            if (!parse_float_field(&p, &y)) continue;
            if (!parse_float_field(&p, &z)) continue;

            if (pos_count + 3 > pos_cap) {
                int nc = pos_cap ? pos_cap * 2 : 1024;
                pos = xrealloc(pos, nc * sizeof(float));
                pos_cap = nc;
            }

            pos[pos_count++] = x;
            pos[pos_count++] = y;
            pos[pos_count++] = z;
            continue;
        }

        if (line[0] == 'v' && line[1] == 't' && is_ws(line[2])) {
            const char* p = line + 2;
            float u, v;
            if (!parse_float_field(&p, &u)) continue;
            if (!parse_float_field(&p, &v)) continue;

            if (tex_count + 2 > tex_cap) {
                int nc = tex_cap ? tex_cap * 2 : 1024;
                tex = xrealloc(tex, nc * sizeof(float));
                tex_cap = nc;
            }

            tex[tex_count++] = u;
            tex[tex_count++] = 1.0f - v;
            continue;
        }

        if (line[0] == 'v' && line[1] == 'n' && is_ws(line[2])) {
            const char* p = line + 2;
            float x, y, z;
            if (!parse_float_field(&p, &x)) continue;
            if (!parse_float_field(&p, &y)) continue;
            if (!parse_float_field(&p, &z)) continue;

            if (nrm_count + 3 > nrm_cap) {
                int nc = nrm_cap ? nrm_cap * 2 : 1024;
                nrm = xrealloc(nrm, nc * sizeof(float));
                nrm_cap = nc;
            }

            nrm[nrm_count++] = x;
            nrm[nrm_count++] = y;
            nrm[nrm_count++] = z;
            continue;
        }

        if (line[0] == 'f' && is_ws(line[1])) {
            const char* p = line + 1;
            int fn = 0;

            while (p < end) {
                skip_ws(&p);
                if (p >= end || *p == '#') break;

                const char* a = p;
                while (p < end && !is_ws(*p) && *p != '#') p++;

                ObjIndex idx;
                if (parse_face_token_range(a, p, &idx)) {
                    if (fn >= face_cap) {
                        face_cap *= 2;
                        face_tmp = xrealloc(face_tmp, face_cap * sizeof(ObjIndex));
                    }
                    face_tmp[fn++] = idx;
                }
            }

            if (fn < 3) continue;

            for (int i = 1; i + 1 < fn; i++) {
                ObjIndex tri[3] = { face_tmp[0], face_tmp[i], face_tmp[i + 1] };

                for (int k = 0; k < 3; k++) {
                    int vix = resolve_index(tri[k].v, pos_count / 3);
                    int vtix = -1;
                    int vnix = -1;

                    ASSERT(vix >= 0, "bad v index");
                    ASSERT(vix < pos_count / 3, "bad v index");

                    if (tri[k].vt != 0) {
                        vtix = resolve_index(tri[k].vt, tex_count / 2);
                        ASSERT(vtix >= 0, "bad vt index");
                        ASSERT(vtix < tex_count / 2, "bad vt index");
                    }

                    if (tri[k].vn != 0) {
                        vnix = resolve_index(tri[k].vn, nrm_count / 3);
                        ASSERT(vnix >= 0, "bad vn index");
                        ASSERT(vnix < nrm_count / 3, "bad vn index");
                    }

                    ObjIndex key = { vix, vtix, vnix };

                    int is_new = 0;
                    int out = map_get_or_add(&map, key, vertices_count / 8, &is_new);

                    if (is_new) {
                        if (vertices_count + 8 > vertices_cap) {
                            int nc = vertices_cap ? vertices_cap * 2 : 1024;
                            vertices = xrealloc(vertices, nc * sizeof(float));
                            vertices_cap = nc;
                        }

                        vertices[vertices_count++] = pos[vix * 3 + 0];
                        vertices[vertices_count++] = pos[vix * 3 + 1];
                        vertices[vertices_count++] = pos[vix * 3 + 2];

                        if (vtix >= 0) {
                            vertices[vertices_count++] = tex[vtix * 2 + 0];
                            vertices[vertices_count++] = tex[vtix * 2 + 1];
                        } else {
                            vertices[vertices_count++] = 0;
                            vertices[vertices_count++] = 0;
                        }

                        if (vnix >= 0) {
                            vertices[vertices_count++] = nrm[vnix * 3 + 0];
                            vertices[vertices_count++] = nrm[vnix * 3 + 1];
                            vertices[vertices_count++] = nrm[vnix * 3 + 2];
                        } else {
                            vertices[vertices_count++] = 0;
                            vertices[vertices_count++] = 1;
                            vertices[vertices_count++] = 0;
                        }
                    }

                    if (indices_count + 1 > indices_cap) {
                        int nc = indices_cap ? indices_cap * 2 : 1024;
                        indices = xrealloc(indices, nc * sizeof(int));
                        indices_cap = nc;
                    }

                    indices[indices_count++] = out;
                }
            }
        }
    }

    Render_request* r = (Render_request*)malloc(sizeof(Render_request));
    memset(r, 0, sizeof(*r));

    r->data = vertices;
    r->data_size = vertices_count * sizeof(float);
    r->triangles = indices;
    r->tri_count = indices_count / 3;

    r->pos[0] = r->pos[1] = r->pos[2] = 0;
    r->rot[0] = r->rot[1] = r->rot[2] = 0;
    r->scale[0] = r->scale[1] = r->scale[2] = 1;

    gfx_packet_static_request(r);

    free(pos);
    free(nrm);
    free(tex);
    free(face_tmp);
    map_free(&map);
    free(f.data);

    return r;
}