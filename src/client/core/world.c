#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "core/world.h"
#include "core/world_gen.h"
#include "core/light.h"
#include "utils/rand.h"
#include "gl/tex.h"
#include "utils/log.h"
#include "utils/noise.h"
#include "utils/file.h"
#include "core/game.h"
#include "core/player.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#ifdef _WIN32
    #include <direct.h>
    #define world_mkdir_raw(p) _mkdir(p)
#else
    #define world_mkdir_raw(p) mkdir((p), 0755)
#endif

typedef struct {
    int cx, cz;
    uint16_t* data;
} ChunkOverride;

static ChunkOverride* g_overrides        = NULL;
static int            g_override_count    = 0;
static int            g_override_capacity = 0;

static void world_install_chunk(World* world, int cx, int cz, uint16_t* data);
static void world_queue_rebuild_3x3(World* world, int cx, int cz);

static inline int floor_div(int a, int b) {
    if (a >= 0) return a / b;
    return - ((-a + b - 1) / b);
}

static inline int floor_mod(int a, int b) {
    int r = a % b;
    if (r < 0) r += b;
    return r;
}

static void wr_u16(uint8_t* b, size_t* o, uint16_t v) {
    b[(*o)++] = (uint8_t)(v);
    b[(*o)++] = (uint8_t)(v >> 8);
}
static void wr_u32(uint8_t* b, size_t* o, uint32_t v) {
    b[(*o)++] = (uint8_t)(v);
    b[(*o)++] = (uint8_t)(v >> 8);
    b[(*o)++] = (uint8_t)(v >> 16);
    b[(*o)++] = (uint8_t)(v >> 24);
}
static void wr_u64(uint8_t* b, size_t* o, uint64_t v) {
    for (int i = 0; i < 8; i++) b[(*o)++] = (uint8_t)(v >> (i * 8));
}
static uint16_t rd_u16(const uint8_t* b, size_t* o) {
    uint16_t v = (uint16_t)b[*o] | ((uint16_t)b[*o+1] << 8); *o += 2; return v;
}
static uint32_t rd_u32(const uint8_t* b, size_t* o) {
    uint32_t v = (uint32_t)b[*o] | ((uint32_t)b[*o+1] << 8) |
                 ((uint32_t)b[*o+2] << 16) | ((uint32_t)b[*o+3] << 24);
    *o += 4; return v;
}
static uint64_t rd_u64(const uint8_t* b, size_t* o) {
    uint64_t v = 0;
    for (int i = 0; i < 8; i++) v |= ((uint64_t)b[*o+i]) << (i * 8);
    *o += 8; return v;
}

static size_t rle_compress(const uint16_t* src, int count, uint8_t* dst, size_t dst_max) {
    int pairs = 0;
    for (int i = 0; i < count; ) {
        int len = 1;
        while (i + len < count && src[i + len] == src[i] && len < 65535) len++;
        i += len;
        pairs++;
    }
    size_t needed = 4 + (size_t)pairs * 4;
    if (needed > dst_max) return 0;

    size_t off = 0;
    dst[off++] = (uint8_t)(pairs);
    dst[off++] = (uint8_t)(pairs >> 8);
    dst[off++] = (uint8_t)(pairs >> 16);
    dst[off++] = (uint8_t)(pairs >> 24);

    for (int i = 0; i < count; ) {
        int len = 1;
        while (i + len < count && src[i + len] == src[i] && len < 65535) len++;
        dst[off++] = (uint8_t)(len);
        dst[off++] = (uint8_t)(len >> 8);
        dst[off++] = (uint8_t)(src[i]);
        dst[off++] = (uint8_t)(src[i] >> 8);
        i += len;
    }
    return off;
}

static int rle_decompress(const uint8_t* src, size_t src_size, uint16_t* dst, int dst_count) {
    if (src_size < 4) return 0;
    uint32_t pairs = (uint32_t)src[0] | ((uint32_t)src[1] << 8) |
                     ((uint32_t)src[2] << 16) | ((uint32_t)src[3] << 24);
    size_t off = 4;
    int out = 0;
    for (uint32_t p = 0; p < pairs; p++) {
        if (off + 4 > src_size) return 0;
        uint16_t len = (uint16_t)src[off] | ((uint16_t)src[off + 1] << 8);
        uint16_t val = (uint16_t)src[off + 2] | ((uint16_t)src[off + 3] << 8);
        off += 4;
        if (out + len > dst_count) return 0;
        for (int k = 0; k < len; k++) dst[out++] = val;
    }
    return out == dst_count;
}

static int world_is_dir(const char* p) {
    struct stat st;
    if (stat(p, &st) != 0) return 0;
    return (st.st_mode & S_IFMT) == S_IFDIR;
}

static int world_is_file(const char* p) {
    struct stat st;
    if (stat(p, &st) != 0) return 0;
    return (st.st_mode & S_IFMT) == S_IFREG;
}

static int world_mkdir_p(const char* path) {
    char buf[1024];
    size_t len = strlen(path);
    if (len == 0 || len >= sizeof(buf)) return 0;
    memcpy(buf, path, len + 1);

    for (size_t i = 1; i < len; i++) {
        if (buf[i] == '/' || buf[i] == '\\') {
            char sep = buf[i];
            buf[i] = '\0';
            if (!world_is_dir(buf) && world_mkdir_raw(buf) != 0 && !world_is_dir(buf))
                return 0;
            buf[i] = sep;
        }
    }
    if (!world_is_dir(buf) && world_mkdir_raw(buf) != 0 && !world_is_dir(buf))
        return 0;
    return 1;
}

static const char* world_basename(const char* p) {
    const char* s = strrchr(p, '/');
    return s ? s + 1 : p;
}

static ChunkOverride* override_find(int cx, int cz) {
    for (int i = 0; i < g_override_count; i++)
        if (g_overrides[i].cx == cx && g_overrides[i].cz == cz)
            return &g_overrides[i];
    return NULL;
}

static ChunkOverride* override_add(int cx, int cz, uint16_t* data) {
    if (g_override_count >= g_override_capacity) {
        g_override_capacity = g_override_capacity ? g_override_capacity * 2 : 64;
        g_overrides = realloc(g_overrides, (size_t)g_override_capacity * sizeof(*g_overrides));
    }
    ChunkOverride* o = &g_overrides[g_override_count++];
    o->cx = cx; o->cz = cz; o->data = data;
    return o;
}

static void override_clear_all(void) {
    for (int i = 0; i < g_override_count; i++) free(g_overrides[i].data);
    free(g_overrides);
    g_overrides = NULL; g_override_count = 0; g_override_capacity = 0;
}

static void world_snapshot_chunk(World* world, int idx) {
    Chunk* chunk = &world->chunks_map[idx];
    if (!chunk->data) return;
    int cx = (int)world->positions_map[idx][0];
    int cz = (int)world->positions_map[idx][1];

    ChunkOverride* ov = override_find(cx, cz);
    if (ov) {
        memcpy(ov->data, chunk->data, CHUNK_DATA_BYTES);
    } else {
        uint16_t* copy = malloc(CHUNK_DATA_BYTES);
        if (!copy) return;
        memcpy(copy, chunk->data, CHUNK_DATA_BYTES);
        override_add(cx, cz, copy);
    }
}

static int world_write_dat(World* world, const char* world_path) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/world.dat", world_path);

    size_t cap = 256 + (size_t)world->pending_block_count * sizeof(BlockChange);
    uint8_t* buf = malloc(cap);
    if (!buf) return 0;

    size_t off = 0;

    memcpy(buf + off, WORLD_MAGIC, WORLD_MAGIC_LEN); off += WORLD_MAGIC_LEN;

    size_t sc_off = off; wr_u16(buf, &off, 0);
    size_t ts_off = 0;
    wr_u16(buf, &off, WORLD_FORMAT_VERSION);
    wr_u16(buf, &off, 0);
    wr_u16(buf, &off, 0);
    ts_off = off;    wr_u32(buf, &off, 0);

    uint16_t sections = 0;

    memcpy(buf + off, "SEED", 4); off += 4;
    wr_u32(buf, &off, 8);
    wr_u64(buf, &off, rng_get_world_seed());
    sections++;

    memcpy(buf + off, "PLYR", 4); off += 4;
    wr_u32(buf, &off, 32);
    {
        float p[3] = { player.camera.pos[0], player.camera.pos[1], player.camera.pos[2] };
        memcpy(buf + off, p, sizeof(p)); off += sizeof(p);

        float pitch = player.camera.rot[0];
        float yaw   = player.camera.rot[1];
        memcpy(buf + off, &yaw,   4); off += 4;
        memcpy(buf + off, &pitch, 4); off += 4;

        wr_u32(buf, &off, 0);
        memset(buf + off, 0, 8); off += 8;
    }
    sections++;

    memcpy(buf + off, "GENV", 4); off += 4;
    wr_u32(buf, &off, 4);
    wr_u32(buf, &off, (uint32_t)WORLD_GEN_VERSION);
    sections++;

    memcpy(buf + off, "CHGS", 4); off += 4;
    {
        uint32_t len = (uint32_t)(world->pending_block_count * sizeof(BlockChange));
        wr_u32(buf, &off, len);
        if (len) { memcpy(buf + off, world->pending_block_changes, len); off += len; }
    }
    sections++;

    {
        const char* v = "2.0.0";
        uint32_t len = (uint32_t)strlen(v);
        memcpy(buf + off, "VERS", 4); off += 4;
        wr_u32(buf, &off, len);
        memcpy(buf + off, v, len); off += len;
        sections++;
    }

    buf[sc_off + 0] = (uint8_t)(sections);
    buf[sc_off + 1] = (uint8_t)(sections >> 8);
    buf[ts_off + 0] = (uint8_t)(off);
    buf[ts_off + 1] = (uint8_t)(off >> 8);
    buf[ts_off + 2] = (uint8_t)(off >> 16);
    buf[ts_off + 3] = (uint8_t)(off >> 24);

    FILE* f = fopen(path, "wb");
    if (!f) { free(buf); return 0; }
    fwrite(buf, 1, off, f);
    fclose(f);
    free(buf);
    return 1;
}

static int world_read_dat(World* world, const char* world_path) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/world.dat", world_path);

    FILE* f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz < 16) { fclose(f); return 0; }
    uint8_t* buf = malloc((size_t)sz);
    if (!buf) { fclose(f); return 0; }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) { free(buf); fclose(f); return 0; }
    fclose(f);

    if (memcmp(buf, WORLD_MAGIC, WORLD_MAGIC_LEN) != 0) {
        free(buf); return 0;
    }

    size_t off = WORLD_MAGIC_LEN;
    uint16_t version       = rd_u16(buf, &off);
    uint16_t section_count = rd_u16(buf, &off);
    rd_u16(buf, &off);
    rd_u16(buf, &off);
    rd_u32(buf, &off);
    (void)version;

    for (int i = 0; i < section_count; i++) {
        if (off + 8 > (size_t)sz) break;
        char tag[4];
        memcpy(tag, buf + off, 4); off += 4;
        uint32_t len = rd_u32(buf, &off);
        size_t payload = off;

        if      (!memcmp(tag, "SEED", 4) && len >= 8) {
            rng_seed(rd_u64(buf, &off));
        }
        else if (!memcmp(tag, "PLYR", 4) && len >= 28) {
            float p[3], yaw, pitch;
            memcpy(p,     buf + off, 12); off += 12;
            memcpy(&yaw,  buf + off, 4);  off += 4;
            memcpy(&pitch,buf + off, 4);  off += 4;

            vec3 pos = { p[0], p[1], p[2] };
            player_set_pos_rot(&player, pos, pitch, yaw);
        }
        else if (!memcmp(tag, "GENV", 4) && len >= 4) {
            world->gen_version = rd_u32(buf, &off);
        }
        else if (!memcmp(tag, "CHGS", 4)) {
            int n = (int)(len / sizeof(BlockChange));
            if (world->pending_block_capacity < n) {
                world->pending_block_capacity = n;
                world->pending_block_changes = realloc(world->pending_block_changes,
                    (size_t)n * sizeof(BlockChange));
            }
            memcpy(world->pending_block_changes, buf + off, len);
            world->pending_block_count = n;
        }

        off = payload + len;
    }

    free(buf);
    return 1;
}

static int world_save_region(World* world, const char* path, int rx, int rz) {
    (void)world;
    const int slots = REGION_SIZE * REGION_SIZE;

    uint8_t* comp_buf = malloc(4 + (size_t)CHUNK_BLOCK_COUNT * 4);
    if (!comp_buf) return 0;

    FILE* f = fopen(path, "wb");
    if (!f) { free(comp_buf); return 0; }

    const uint32_t header_size = 20;

    uint8_t hdr[14];
    size_t ho = 0;
    wr_u16(hdr, &ho, REGION_FORMAT_VERSION);
    wr_u16(hdr, &ho, (uint16_t)slots);
    wr_u16(hdr, &ho, 0);
    wr_u32(hdr, &ho, header_size);
    wr_u32(hdr, &ho, 0);

    fwrite(REGION_MAGIC, 1, REGION_MAGIC_LEN, f);
    fwrite(hdr, 1, sizeof(hdr), f);

    long index_pos = ftell(f);
    uint8_t zero_entry[16] = {0};
    for (int i = 0; i < slots; i++) fwrite(zero_entry, 1, 16, f);

    uint32_t* offsets = malloc((size_t)slots * sizeof(uint32_t));
    uint32_t* sizes   = malloc((size_t)slots * sizeof(uint32_t));
    uint32_t* raws    = malloc((size_t)slots * sizeof(uint32_t));
    uint16_t* comps   = malloc((size_t)slots * sizeof(uint16_t));
    if (!offsets || !sizes || !raws || !comps) {
        fclose(f);
        free(comp_buf); free(offsets); free(sizes); free(raws); free(comps);
        return 0;
    }

    for (int lz = 0; lz < REGION_SIZE; lz++) {
        for (int lx = 0; lx < REGION_SIZE; lx++) {
            int cx = rx * REGION_SIZE + lx;
            int cz = rz * REGION_SIZE + lz;
            int slot = lz * REGION_SIZE + lx;

            ChunkOverride* ov = override_find(cx, cz);
            if (!ov) {
                comps[slot] = 0;
                offsets[slot] = 0;
                sizes[slot] = 0;
                raws[slot] = 0;
                continue;
            }

            size_t rle_size = rle_compress(ov->data, CHUNK_BLOCK_COUNT,
                                           comp_buf, 4 + (size_t)CHUNK_BLOCK_COUNT * 4);
            offsets[slot] = (uint32_t)ftell(f);
            if (rle_size > 0 && rle_size < CHUNK_DATA_BYTES) {
                comps[slot] = 1;
                sizes[slot] = (uint32_t)rle_size;
                raws[slot] = CHUNK_DATA_BYTES;
                fwrite(comp_buf, 1, rle_size, f);
            } else {
                comps[slot] = 0;
                sizes[slot] = CHUNK_DATA_BYTES;
                raws[slot] = CHUNK_DATA_BYTES;
                fwrite(ov->data, 1, CHUNK_DATA_BYTES, f);
            }
        }
    }

    fseek(f, index_pos, SEEK_SET);
    for (int lz = 0; lz < REGION_SIZE; lz++) {
        for (int lx = 0; lx < REGION_SIZE; lx++) {
            int slot = lz * REGION_SIZE + lx;
            uint8_t e[16]; size_t eo = 0;
            e[eo++] = (uint8_t)lx;
            e[eo++] = (uint8_t)lz;
            wr_u16(e, &eo, comps[slot]);
            wr_u32(e, &eo, offsets[slot]);
            wr_u32(e, &eo, sizes[slot]);
            wr_u32(e, &eo, raws[slot]);
            fwrite(e, 1, 16, f);
        }
    }

    fclose(f);
    free(comp_buf);
    free(offsets); free(sizes); free(raws); free(comps);
    return 1;
}

static int world_load_region(const char* path) {
    const char* base = world_basename(path);
    int rx, rz;
    if (sscanf(base, "r.%d.%d.dat", &rx, &rz) != 2) return 0;

    FILE* f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz < 20) { fclose(f); return 0; }
    uint8_t* buf = malloc((size_t)sz);
    if (!buf || fread(buf, 1, (size_t)sz, f) != (size_t)sz) { free(buf); fclose(f); return 0; }
    fclose(f);

    if (memcmp(buf, REGION_MAGIC, REGION_MAGIC_LEN) != 0) { free(buf); return 0; }

    size_t off = REGION_MAGIC_LEN;
    uint16_t ver   = rd_u16(buf, &off);
    uint16_t count = rd_u16(buf, &off);
    rd_u16(buf, &off);
    uint32_t idx   = rd_u32(buf, &off);
    rd_u32(buf, &off);
    (void)ver;

    for (int i = 0; i < count; i++) {
        if (idx + 16 > (size_t)sz) break;
        size_t eo = idx;
        uint8_t lx = buf[eo++];
        uint8_t lz = buf[eo++];
        uint16_t compression = rd_u16(buf, &eo);
        uint32_t data_off    = rd_u32(buf, &eo);
        uint32_t data_sz     = rd_u32(buf, &eo);
        uint32_t raw_sz      = rd_u32(buf, &eo);
        idx = eo;

        if (data_off == 0) continue;

        int cx = rx * REGION_SIZE + lx;
        int cz = rz * REGION_SIZE + lz;
        if (override_find(cx, cz)) continue;

        if (raw_sz != CHUNK_DATA_BYTES) continue;
        if ((size_t)data_off + data_sz > (size_t)sz) continue;

        uint16_t* data = malloc(CHUNK_DATA_BYTES);
        if (!data) continue;

        if (compression == 0) {
            if (data_sz != CHUNK_DATA_BYTES) { free(data); continue; }
            memcpy(data, buf + data_off, CHUNK_DATA_BYTES);
            override_add(cx, cz, data);
        } else if (compression == 1) {
            if (!rle_decompress(buf + data_off, data_sz, data, CHUNK_BLOCK_COUNT)) {
                free(data);
                continue;
            }
            override_add(cx, cz, data);
        } else {
            free(data);
        }
    }

    free(buf);
    return 1;
}

static void world_load_all_regions(const char* data_path) {
    DIR* d = opendir(data_path);
    if (!d) return;
    struct dirent* e;
    while ((e = readdir(d)) != NULL) {
        size_t n = strlen(e->d_name);
        if (n < 8) continue;
        if (!(e->d_name[0] == 'r' && e->d_name[1] == '.')) continue;
        if (strcmp(e->d_name + n - 4, ".dat") != 0) continue;
        char full[1024];
        snprintf(full, sizeof(full), "%s/%s", data_path, e->d_name);
        world_load_region(full);
    }
    closedir(d);
}

void world_init(World* world) {
    world->chunks_map = malloc(sizeof(Chunk) * MAX_LOADED_CHUNKS);
    world->positions_map = malloc(sizeof(vec2) * MAX_LOADED_CHUNKS);
    world->index_map = malloc(sizeof(int) * MAX_LOADED_CHUNKS);

    world->pending_block_capacity = 64000;
    world->pending_block_changes = malloc(sizeof(BlockChange) * world->pending_block_capacity);
    world->pending_block_count = 0;

    noise_set_seed(rng_get_world_seed());

    for (int i = 0; i < MAX_LOADED_CHUNKS; i++) {
        world->index_map[i] = -1;
    }

    world->pending_count = 0;

    world->gen_version = (uint32_t)WORLD_GEN_VERSION;

    world_gen_start();
}

static int world_load_legacy_file(World* world, const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz < 6 + 8 + 20 + 4) { fclose(f); return 0; }
    uint8_t* buf = malloc((size_t)sz);
    if (!buf || fread(buf, 1, (size_t)sz, f) != (size_t)sz) { free(buf); fclose(f); return 0; }
    fclose(f);

    if (memcmp(buf, "CCRAFT", 6) != 0) { free(buf); return 0; }

    size_t off = 6;
    uint64_t seed = rd_u64(buf, &off); rng_seed(seed);
    off += 20;
    int count = (int)rd_u32(buf, &off);
    if (count < 0 || off + (size_t)count * sizeof(BlockChange) > (size_t)sz) {
        free(buf); return 0;
    }
    if (world->pending_block_capacity < count) {
        world->pending_block_capacity = count;
        world->pending_block_changes = realloc(world->pending_block_changes,
            (size_t)count * sizeof(BlockChange));
    }
    memcpy(world->pending_block_changes, buf + off, (size_t)count * sizeof(BlockChange));
    world->pending_block_count = count;

    world->gen_version = 0;
    free(buf);
    return 1;
}

int world_exists(const char* world_path) {
    return world_is_dir(world_path) || world_is_file(world_path);
}

void world_load(World* world, const char* world_path) {
    override_clear_all();

    if (world_is_file(world_path)) {
        if (!world_load_legacy_file(world, world_path)) {
            printf("world_load: %s is not a valid legacy world\n", world_path);
            return;
        }
        char backup[2048];
        snprintf(backup, sizeof(backup), "%s.legacy", world_path);
        if (rename(world_path, backup) != 0) {
            printf("world_load: cannot migrate %s (rename failed)\n", world_path);
            return;
        }
        world_save(world, world_path);
        return;
    }

    char data_path[1024];
    snprintf(data_path, sizeof(data_path), "%s/data", world_path);

    if (!world_read_dat(world, world_path)) {
        printf("world_load: cannot read %s/world.dat\n", world_path);
        return;
    }

    if (world->gen_version != (uint32_t)WORLD_GEN_VERSION) {
        printf("world_load: generator version changed (%u -> %u); loaded chunks keep their saved state\n",
               world->gen_version, (uint32_t)WORLD_GEN_VERSION);
    }

    world_load_all_regions(data_path);
    world->gen_version = (uint32_t)WORLD_GEN_VERSION;
}

void world_save(World* world, const char* world_path) {
    if (!world_mkdir_p(world_path)) {
        printf("world_save: cannot create %s\n", world_path);
        return;
    }
    char data_path[1024];
    snprintf(data_path, sizeof(data_path), "%s/data", world_path);
    world_mkdir_p(data_path);

    for (int i = 0; i < MAX_LOADED_CHUNKS; i++) {
        if (world->index_map[i] == -1) continue;
        world_snapshot_chunk(world, i);
    }

    {
        int write_idx = 0;
        for (int k = 0; k < world->pending_block_count; k++) {
            BlockChange* bc = &world->pending_block_changes[k];
            int cx = floor_div(bc->x, CHUNK_WIDTH);
            int cz = floor_div(bc->z, CHUNK_DEPTH);
            if (override_find(cx, cz)) continue;
            world->pending_block_changes[write_idx++] = *bc;
        }
        world->pending_block_count = write_idx;
    }

    world_write_dat(world, world_path);

    int max_r = g_override_count > 0 ? g_override_count : 1;
    int (*regions)[2] = malloc((size_t)max_r * sizeof(int[2]));
    if (!regions) return;
    int rc = 0;

    for (int i = 0; i < g_override_count; i++) {
        int rx = floor_div(g_overrides[i].cx, REGION_SIZE);
        int rz = floor_div(g_overrides[i].cz, REGION_SIZE);
        int seen = 0;
        for (int j = 0; j < rc; j++) if (regions[j][0] == rx && regions[j][1] == rz) { seen = 1; break; }
        if (!seen) { regions[rc][0] = rx; regions[rc][1] = rz; rc++; }
    }

    for (int i = 0; i < rc; i++) {
        char path[1024];
        snprintf(path, sizeof(path), "%s/r.%d.%d.dat", data_path, regions[i][0], regions[i][1]);
        world_save_region(world, path, regions[i][0], regions[i][1]);
    }
    free(regions);
}

void new_world(const char* world_path) {
    override_clear_all();
    world_mkdir_p(world_path);
    char data_path[1024];
    snprintf(data_path, sizeof(data_path), "%s/data", world_path);
    world_mkdir_p(data_path);

    rng_seed(rng_get_world_seed());

    World dummy = {0};
    dummy.pending_block_changes = NULL;
    dummy.pending_block_count   = 0;
    dummy.gen_version = (uint32_t)WORLD_GEN_VERSION;
    world_write_dat(&dummy, world_path);
}

void world_add_chunk(World* world, Chunk* chunk, vec2 position) {
    for (int i = 0; i < MAX_LOADED_CHUNKS; i++) {
        if (world->index_map[i] != -1) {
            if (world->positions_map[i][0] == position[0] &&
                world->positions_map[i][1] == position[1]) {
                world->chunks_map[i] = *chunk;
                return;
            }
        }
    }

    for (int i = 0; i < MAX_LOADED_CHUNKS; i++) {
        if (world->index_map[i] == -1) {
            world->chunks_map[i] = *chunk;
            world->positions_map[i][0] = position[0];
            world->positions_map[i][1] = position[1];
            world->index_map[i] = 1;

            world->chunks_map[i].model.pos[0] = position[0] * CHUNK_WIDTH;
            world->chunks_map[i].model.pos[1] = 0.0f;
            world->chunks_map[i].model.pos[2] = position[1] * CHUNK_DEPTH;

            world->chunks_map[i].water_model.pos[0] = position[0] * CHUNK_WIDTH;
            world->chunks_map[i].water_model.pos[1] = 0.0f;
            world->chunks_map[i].water_model.pos[2] = position[1] * CHUNK_DEPTH;
            return;
        }
    }

    printf("Cant add new chunks, world overflow.\n");
}

Chunk* world_get_chunk(World* world, int cx, int cz) {
    for (int i = 0; i < MAX_LOADED_CHUNKS; i++) {
        if (world->index_map[i] != -1) {
            if ((int)world->positions_map[i][0] == cx &&
                (int)world->positions_map[i][1] == cz) {
                return &world->chunks_map[i];
            }
        }
    }
    return NULL;
}

uint16_t world_get_block(World* world, int wx, int wy, int wz) {
    if (wy < 0 || wy >= CHUNK_HEIGHT)
        return AIR;

    int cx = floor_div(wx, CHUNK_WIDTH);
    int cz = floor_div(wz, CHUNK_DEPTH);

    Chunk* chunk = world_get_chunk(world, cx, cz);
    if (!chunk)
        return AIR;

    int lx = floor_mod(wx, CHUNK_WIDTH);
    int lz = floor_mod(wz, CHUNK_DEPTH);

    int index = lx + CHUNK_WIDTH * (wy + CHUNK_HEIGHT * lz);
    return chunk->data[index];
}

uint16_t world_get_block_at(World* world, vec3 p) {
    int wx = (int)floorf(p[0]);
    int wy = (int)floorf(p[1]);
    int wz = (int)floorf(p[2]);

    return world_get_block(world, wx, wy, wz);
}

void world_queue_block_change(World* world, int x, int y, int z, uint16_t block) {
    if (world->pending_block_count >= world->pending_block_capacity) {
        world->pending_block_capacity *= 2;
        world->pending_block_changes = realloc(world->pending_block_changes, 
            sizeof(BlockChange) * world->pending_block_capacity);
        if (!world->pending_block_changes) {
            printf("Failed to expand block change buffer\n");
            return;
        }
    }

    for (int i = 0; i < world->pending_block_count; i++) {
        if (world->pending_block_changes[i].x == x && 
            world->pending_block_changes[i].y == y &&
            world->pending_block_changes[i].z == z) {
            world->pending_block_changes[i].block = block;
            return;
        }
    }

    world->pending_block_changes[world->pending_block_count].x = x;
    world->pending_block_changes[world->pending_block_count].y = y;
    world->pending_block_changes[world->pending_block_count].z = z;
    world->pending_block_changes[world->pending_block_count].block = block;
    world->pending_block_count++;
}

int world_set_block(World* world, int wx, int wy, int wz, uint16_t block) {
    if (wy < 0 || wy >= CHUNK_HEIGHT)
        return 0;

    int cx = floor_div(wx, CHUNK_WIDTH);
    int cz = floor_div(wz, CHUNK_DEPTH);

    Chunk* chunk = world_get_chunk(world, cx, cz);
    if (!chunk) {
        world_queue_block_change(world, wx, wy, wz, block);

        ChunkOverride* ov = override_find(cx, cz);
        if (ov) {
            int lx = floor_mod(wx, CHUNK_WIDTH);
            int lz = floor_mod(wz, CHUNK_DEPTH);
            int index = lx + CHUNK_WIDTH * (wy + CHUNK_HEIGHT * lz);
            ov->data[index] = block;
        }
        return 1;
    }

    int lx = floor_mod(wx, CHUNK_WIDTH);
    int lz = floor_mod(wz, CHUNK_DEPTH);

    int index = lx + CHUNK_WIDTH * (wy + CHUNK_HEIGHT * lz);
    chunk->data[index] = block;

    world_queue_rebuild_3x3(world, cx, cz);

    return 1;
}

void rebuild_chunks_for_block(World* world, int wx, int wy, int wz) {
    (void)wy;
    int cx = floor_div(wx, CHUNK_WIDTH);
    int cz = floor_div(wz, CHUNK_DEPTH);
    world_queue_rebuild_3x3(world, cx, cz);
}

void world_set_block_at(World* world, vec3 p, uint16_t block) {
    int wx = (int)floorf(p[0]);
    int wy = (int)floorf(p[1]);
    int wz = (int)floorf(p[2]);

    world_set_block(world, wx, wy, wz, block);
}

static void world_invalidate_chunk_mesh(Chunk* chunk) {
    if (chunk)
        chunk->mesh_generation++;
}

void world_queue_rebuild(World* world, int cx, int cz) {
    if (world->pending_count >= MAX_PENDING_REBUILDS)
        return;

    for (int i = 0; i < world->pending_count; i++) {
        if ((int)world->pending_rebuilds[i][0] == cx &&
            (int)world->pending_rebuilds[i][1] == cz) {
            return;
        }
    }

    world_invalidate_chunk_mesh(world_get_chunk(world, cx, cz));

    world->pending_rebuilds[world->pending_count][0] = (float)cx;
    world->pending_rebuilds[world->pending_count][1] = (float)cz;
    world->pending_count++;
}

static void world_queue_rebuild_3x3(World* world, int cx, int cz) {
    for (int dx = -1; dx <= 1; dx++) {
        for (int dz = -1; dz <= 1; dz++)
            world_queue_rebuild(world, cx + dx, cz + dz);
    }
}

void world_process_rebuild_queue(World* world) {
    int submitted = 0;

    for (int i = 0; i < world->pending_count && submitted < 8; ) {
        int cx = (int)world->pending_rebuilds[i][0];
        int cz = (int)world->pending_rebuilds[i][1];

        if (!world_get_chunk(world, cx, cz)) {
            for (int j = i; j < world->pending_count - 1; j++) {
                world->pending_rebuilds[j][0] = world->pending_rebuilds[j + 1][0];
                world->pending_rebuilds[j][1] = world->pending_rebuilds[j + 1][1];
            }
            world->pending_count--;
            continue;
        }

        if (world_mesh_in_flight(cx, cz)) {
            i++;
            continue;
        }

        if (!world_mesh_submit(world, cx, cz)) {
            i++;
            continue;
        }

        for (int j = i; j < world->pending_count - 1; j++) {
            world->pending_rebuilds[j][0] = world->pending_rebuilds[j + 1][0];
            world->pending_rebuilds[j][1] = world->pending_rebuilds[j + 1][1];
        }
        world->pending_count--;
        submitted++;
    }
}

void world_rebuild_chunk(World* world, int cx, int cz) {
    world_mesh_submit(world, cx, cz);
}

void world_render(World* world, void* active_program, void* water_program, int cull, int render_water) {
    vec3 cam_pos;
    glm_vec3_copy(player.camera.pos, cam_pos);

    vec3 forward;
    glm_vec3_copy(player.camera.forward, forward);

    float forward_len2 = forward[0] * forward[0] + forward[1] * forward[1] + forward[2] * forward[2];
    if (forward_len2 > 0.000001f) {
        float inv = 1.0f / sqrtf(forward_len2);
        forward[0] *= inv;
        forward[1] *= inv;
        forward[2] *= inv;
    }

    if (!potato_mode) {
        for (int i = 0; i < MAX_LOADED_CHUNKS; i++) {
            if (world->index_map[i] == -1) continue;

            if (cull) {
                vec3 box_min = {
                    world->chunks_map[i].model.pos[0],
                    0.0f,
                    world->chunks_map[i].model.pos[2]
                };
                vec3 box_max = {
                    world->chunks_map[i].model.pos[0] + CHUNK_WIDTH,
                    (float)CHUNK_HEIGHT,
                    world->chunks_map[i].model.pos[2] + CHUNK_DEPTH
                };

                float max_dot = -1e30f;
                for (int ix = 0; ix < 2; ix++) {
                    for (int iy = 0; iy < 2; iy++) {
                        for (int iz = 0; iz < 2; iz++) {
                            vec3 corner = {
                                ix ? box_max[0] : box_min[0],
                                iy ? box_max[1] : box_min[1],
                                iz ? box_max[2] : box_min[2]
                            };
                            float dx = corner[0] - cam_pos[0];
                            float dy = corner[1] - cam_pos[1];
                            float dz = corner[2] - cam_pos[2];
                            float d = forward[0] * dx + forward[1] * dy + forward[2] * dz;
                            if (d > max_dot) max_dot = d;
                        }
                    }
                }
                if (max_dot < 0.0f) continue;
            }

            gfx_render(&world->chunks_map[i].model, (Program*)active_program);
            if (render_water) {
                gfx_render(&world->chunks_map[i].water_model, (Program*)water_program);
            }
        }
        return;
    }

    for (int i = 0; i < MAX_LOADED_CHUNKS; i++) {
        if (world->index_map[i] == -1) continue;

        if (cull) {
            vec3 box_min = {
                world->chunks_map[i].model.pos[0],
                0.0f,
                world->chunks_map[i].model.pos[2]
            };
            vec3 box_max = {
                world->chunks_map[i].model.pos[0] + CHUNK_WIDTH,
                (float)CHUNK_HEIGHT,
                world->chunks_map[i].model.pos[2] + CHUNK_DEPTH
            };

            float max_dot = -1e30f;
            for (int ix = 0; ix < 2; ix++) {
                for (int iy = 0; iy < 2; iy++) {
                    for (int iz = 0; iz < 2; iz++) {
                        vec3 corner = {
                            ix ? box_max[0] : box_min[0],
                            iy ? box_max[1] : box_min[1],
                            iz ? box_max[2] : box_min[2]
                        };
                        float dx = corner[0] - cam_pos[0];
                        float dy = corner[1] - cam_pos[1];
                        float dz = corner[2] - cam_pos[2];
                        float d = forward[0] * dx + forward[1] * dy + forward[2] * dz;
                        if (d > max_dot) max_dot = d;
                    }
                }
            }
            if (max_dot < 0.0f) continue;
        }

        gfx_render(&world->chunks_map[i].model, (Program*)active_program);
    }

    if (!render_water) return;

    WaterSortEntry water_buf[MAX_LOADED_CHUNKS];
    int water_count = 0;

    for (int i = 0; i < MAX_LOADED_CHUNKS; i++) {
        if (world->index_map[i] == -1) continue;

        Chunk* chunk = &world->chunks_map[i];

        if (cull) {
            vec3 box_min = {
                chunk->water_model.pos[0],
                0.0f,
                chunk->water_model.pos[2]
            };
            vec3 box_max = {
                chunk->water_model.pos[0] + CHUNK_WIDTH,
                (float)CHUNK_HEIGHT,
                chunk->water_model.pos[2] + CHUNK_DEPTH
            };

            float max_dot = -1e30f;
            for (int ix = 0; ix < 2; ix++) {
                for (int iy = 0; iy < 2; iy++) {
                    for (int iz = 0; iz < 2; iz++) {
                        vec3 corner = {
                            ix ? box_max[0] : box_min[0],
                            iy ? box_max[1] : box_min[1],
                            iz ? box_max[2] : box_min[2]
                        };
                        float dx = corner[0] - cam_pos[0];
                        float dy = corner[1] - cam_pos[1];
                        float dz = corner[2] - cam_pos[2];
                        float d = forward[0] * dx + forward[1] * dy + forward[2] * dz;
                        if (d > max_dot) max_dot = d;
                    }
                }
            }
            if (max_dot < 0.0f) continue;
        }

        float cx = chunk->water_model.pos[0] + CHUNK_WIDTH * 0.5f;
        float cy = CHUNK_HEIGHT * 0.5f;
        float cz = chunk->water_model.pos[2] + CHUNK_DEPTH * 0.5f;
        float dx = cx - cam_pos[0];
        float dy = cy - cam_pos[1];
        float dz = cz - cam_pos[2];
        float dist2 = dx*dx + dy*dy + dz*dz;

        water_buf[water_count].idx = i;
        water_buf[water_count].dist2 = dist2;
        water_count++;
    }

    for (int i = 0; i < water_count - 1; i++) {
        int max_idx = i;
        for (int j = i + 1; j < water_count; j++) {
            if (water_buf[j].dist2 > water_buf[max_idx].dist2)
                max_idx = j;
        }
        if (max_idx != i) {
            WaterSortEntry tmp = water_buf[i];
            water_buf[i] = water_buf[max_idx];
            water_buf[max_idx] = tmp;
        }
    }

    for (int k = 0; k < water_count; k++) {
        int i = water_buf[k].idx;
        gfx_render(&world->chunks_map[i].water_model, (Program*)water_program);
    }
}

void world_render_water(World* world, void* program, int cull, void* background_tex) {
    vec3 cam_pos;
    glm_vec3_copy(player.camera.pos, cam_pos);

    vec3 forward;
    glm_vec3_copy(player.camera.forward, forward);

    float forward_len2 = forward[0] * forward[0] + forward[1] * forward[1] + forward[2] * forward[2];
    if (forward_len2 > 0.000001f) {
        float inv = 1.0f / sqrtf(forward_len2);
        forward[0] *= inv;
        forward[1] *= inv;
        forward[2] *= inv;
    }

    Program* prog = (Program*)program;
    program_use(prog);
    program_set_int(prog, "background", 0);
    texture_bind((Texture*)background_tex, 0);

    for (int i = 0; i < MAX_LOADED_CHUNKS; i++) {
        if (world->index_map[i] == -1) continue;

        if (cull) {
            vec3 box_min = {
                world->chunks_map[i].water_model.pos[0],
                0.0f,
                world->chunks_map[i].water_model.pos[2]
            };
            vec3 box_max = {
                world->chunks_map[i].water_model.pos[0] + CHUNK_WIDTH,
                (float)CHUNK_HEIGHT,
                world->chunks_map[i].water_model.pos[2] + CHUNK_DEPTH
            };

            float max_dot = -1e30f;

            for (int ix = 0; ix < 2; ix++) {
                for (int iy = 0; iy < 2; iy++) {
                    for (int iz = 0; iz < 2; iz++) {
                        vec3 corner = {
                            ix ? box_max[0] : box_min[0],
                            iy ? box_max[1] : box_min[1],
                            iz ? box_max[2] : box_min[2]
                        };

                        float dx = corner[0] - cam_pos[0];
                        float dy = corner[1] - cam_pos[1];
                        float dz = corner[2] - cam_pos[2];
                        float d = forward[0] * dx + forward[1] * dy + forward[2] * dz;
                        if (d > max_dot) max_dot = d;
                    }
                }
            }

            if (max_dot < 0.0f) continue;
        }
        glDisable(GL_CULL_FACE);
        gfx_render(&world->chunks_map[i].water_model, prog);
        glEnable(GL_CULL_FACE);
    }
}

void world_render_water_only(World* world, void* program, int cull) {
    vec3 cam_pos;
    glm_vec3_copy(player.camera.pos, cam_pos);

    vec3 forward;
    glm_vec3_copy(player.camera.forward, forward);

    float forward_len2 = forward[0] * forward[0] + forward[1] * forward[1] + forward[2] * forward[2];
    if (forward_len2 > 0.000001f) {
        float inv = 1.0f / sqrtf(forward_len2);
        forward[0] *= inv;
        forward[1] *= inv;
        forward[2] *= inv;
    }

    Program* prog = (Program*)program;

    for (int i = 0; i < MAX_LOADED_CHUNKS; i++) {
        if (world->index_map[i] == -1) continue;

        if (cull) {
            vec3 box_min = {
                world->chunks_map[i].water_model.pos[0],
                0.0f,
                world->chunks_map[i].water_model.pos[2]
            };
            vec3 box_max = {
                world->chunks_map[i].water_model.pos[0] + CHUNK_WIDTH,
                (float)CHUNK_HEIGHT,
                world->chunks_map[i].water_model.pos[2] + CHUNK_DEPTH
            };

            float max_dot = -1e30f;

            for (int ix = 0; ix < 2; ix++) {
                for (int iy = 0; iy < 2; iy++) {
                    for (int iz = 0; iz < 2; iz++) {
                        vec3 corner = {
                            ix ? box_max[0] : box_min[0],
                            iy ? box_max[1] : box_min[1],
                            iz ? box_max[2] : box_min[2]
                        };

                        float dx = corner[0] - cam_pos[0];
                        float dy = corner[1] - cam_pos[1];
                        float dz = corner[2] - cam_pos[2];
                        float d = forward[0] * dx + forward[1] * dy + forward[2] * dz;
                        if (d > max_dot) max_dot = d;
                    }
                }
            }

            if (max_dot < 0.0f) continue;
        }

        gfx_render(&world->chunks_map[i].water_model, prog);
    }
}

static void world_install_chunk(World* world, int cx, int cz, uint16_t* data) {
    if (world_get_chunk(world, cx, cz)) {
        free(data);
        return;
    }

    for (int i = 0; i < MAX_LOADED_CHUNKS; i++) {
        if (world->index_map[i] != -1)
            continue;

        Chunk* chunk = &world->chunks_map[i];
        chunk->data = data;
        chunk->sky_light = NULL;
        chunk->block_light = NULL;
        chunk->mesh_generation = 0;
        chunk->model = (Render_request){0};
        chunk->water_model = (Render_request){0};

        ChunkOverride* ov = override_find(cx, cz);
        if (ov)
            memcpy(chunk->data, ov->data, CHUNK_DATA_BYTES);

        for (int k = 0; k < world->pending_block_count; k++) {
            BlockChange* bc = &world->pending_block_changes[k];
            int bcx = floor_div(bc->x, CHUNK_WIDTH);
            int bcz = floor_div(bc->z, CHUNK_DEPTH);
            if (bcx != cx || bcz != cz) continue;
            if (bc->y < 0 || bc->y >= CHUNK_HEIGHT) continue;
            int lx = floor_mod(bc->x, CHUNK_WIDTH);
            int lz = floor_mod(bc->z, CHUNK_DEPTH);
            int index = lx + CHUNK_WIDTH * (bc->y + CHUNK_HEIGHT * lz);
            chunk->data[index] = bc->block;
        }

        world->positions_map[i][0] = (float)cx;
        world->positions_map[i][1] = (float)cz;
        world->index_map[i] = 1;

        chunk->model.pos[0] = (float)cx * CHUNK_WIDTH;
        chunk->model.pos[1] = 0.0f;
        chunk->model.pos[2] = (float)cz * CHUNK_DEPTH;

        chunk->water_model.pos[0] = (float)cx * CHUNK_WIDTH;
        chunk->water_model.pos[1] = 0.0f;
        chunk->water_model.pos[2] = (float)cz * CHUNK_DEPTH;

        world_queue_rebuild_3x3(world, cx, cz);
        return;
    }

    free(data);
    printf("Cant add new chunks, world overflow.\n");
}

void world_reload_render_distance(World* world, vec3 ppos) {
    int pcx = floor_div((int)ppos[0], CHUNK_WIDTH);
    int pcz = floor_div((int)ppos[2], CHUNK_DEPTH);

    int render_dist = WORLD_RENDER_DISTANCE;

    for (int cx = pcx - render_dist; cx <= pcx + render_dist; cx++) {
        for (int cz = pcz - render_dist; cz <= pcz + render_dist; cz++) {
            int dx = cx - pcx;
            int dz = cz - pcz;
            if (dx * dx + dz * dz > render_dist * render_dist)
                continue;
            if (!world_get_chunk(world, cx, cz))
                continue;
            world_queue_rebuild_3x3(world, cx, cz);
        }
    }

    world_process_rebuild_queue(world);
}

void world_tick(World* world, vec3 ppos) {
    int pcx = floor_div((int)ppos[0], CHUNK_WIDTH);
    int pcz = floor_div((int)ppos[2], CHUNK_DEPTH);

    world_gen_set_player_chunk(pcx, pcz);

    int render_dist = WORLD_RENDER_DISTANCE;

    int should_load_count = 0;
    vec2 should_load[1024]; // changed from 256 to 1024, big render distances before that overflow

    for (int cx = pcx - render_dist; cx <= pcx + render_dist; cx++) {
        for (int cz = pcz - render_dist; cz <= pcz + render_dist; cz++) {
            Chunk* existing = world_get_chunk(world, cx, cz);
            if (!existing) {
                int dx = cx - pcx;
                int dz = cz - pcz;
                if (dx * dx + dz * dz <= render_dist * render_dist) {
                    should_load[should_load_count][0] = (float)cx;
                    should_load[should_load_count][1] = (float)cz;
                    should_load_count++;
                }
            }
        }
    }

    for (int i = 0; i < should_load_count - 1; i++) {
        int min_idx = i;
        int mdx = (int)should_load[i][0] - pcx;
        int mdz = (int)should_load[i][1] - pcz;
        int min_dist = mdx * mdx + mdz * mdz;

        for (int j = i + 1; j < should_load_count; j++) {
            int dx = (int)should_load[j][0] - pcx;
            int dz = (int)should_load[j][1] - pcz;
            int d = dx * dx + dz * dz;
            if (d < min_dist) {
                min_dist = d;
                min_idx = j;
            }
        }

        if (min_idx != i) {
            float tx = should_load[i][0];
            float tz = should_load[i][1];
            should_load[i][0] = should_load[min_idx][0];
            should_load[i][1] = should_load[min_idx][1];
            should_load[min_idx][0] = tx;
            should_load[min_idx][1] = tz;
        }
    }

    for (int i = 0; i < MAX_LOADED_CHUNKS; i++) {
        if (world->index_map[i] != -1) {
            int cx = (int)world->positions_map[i][0];
            int cz = (int)world->positions_map[i][1];

            int dx = cx - pcx;
            int dz = cz - pcz;

            if (dx * dx + dz * dz > render_dist * render_dist) {
                world_snapshot_chunk(world, i);

                Chunk* chunk = &world->chunks_map[i];

                if (chunk->data) { free(chunk->data); chunk->data = NULL; }
                chunk_light_free(chunk);
                if (chunk->model.data) { free(chunk->model.data); chunk->model.data = NULL; }
                if (chunk->model.triangles) { free(chunk->model.triangles); chunk->model.triangles = NULL; }
                vbo_free(&chunk->model.cache.vbo);
                vao_free(&chunk->model.cache.vao);
                ebo_free(&chunk->model.cache.ebo);

                if (chunk->water_model.data) { free(chunk->water_model.data); chunk->water_model.data = NULL; }
                if (chunk->water_model.triangles) { free(chunk->water_model.triangles); chunk->water_model.triangles = NULL; }
                vbo_free(&chunk->water_model.cache.vbo);
                vao_free(&chunk->water_model.cache.vao);
                ebo_free(&chunk->water_model.cache.ebo);

                world->index_map[i] = -1;
            }
        }
    }

    int gen_cx, gen_cz;
    uint16_t* gen_data;
    while (world_gen_poll(&gen_cx, &gen_cz, &gen_data)) {
        int dx = gen_cx - pcx;
        int dz = gen_cz - pcz;
        if (dx * dx + dz * dz > render_dist * render_dist) {
            free(gen_data);
            continue;
        }
        world_install_chunk(world, gen_cx, gen_cz, gen_data);
    }

    int mesh_cx, mesh_cz;
    ChunkMeshResult* mesh;
    uint32_t mesh_gen;
    while (world_mesh_poll(&mesh_cx, &mesh_cz, &mesh, &mesh_gen)) {
        Chunk* chunk = world_get_chunk(world, mesh_cx, mesh_cz);
        if (!chunk || chunk->mesh_generation != mesh_gen) {
            chunk_mesh_result_free(mesh);
            continue;
        }
        chunk_apply_mesh(chunk, mesh);
    }

    for (int j = 0; j < should_load_count; j++) {
        int cx = (int)should_load[j][0];
        int cz = (int)should_load[j][1];

        if (!world_get_chunk(world, cx, cz) && !world_gen_in_flight(cx, cz))
            world_gen_submit(cx, cz);
    }

    world_process_rebuild_queue(world);
}

void world_destroy(World* world) {
    world_gen_stop();

    for (int i = 0; i < MAX_LOADED_CHUNKS; i++) {
        if (world->index_map[i] != -1) {
            Chunk* chunk = &world->chunks_map[i];
            if (chunk->data) {
                free(chunk->data);
                chunk->data = NULL;
            }
            if (chunk->model.data) {
                free(chunk->model.data);
                chunk->model.data = NULL;
            }
            if (chunk->model.triangles) {
                free(chunk->model.triangles);
                chunk->model.triangles = NULL;
            }
            vbo_free(&chunk->model.cache.vbo);
            vao_free(&chunk->model.cache.vao);
            ebo_free(&chunk->model.cache.ebo);

            if (chunk->water_model.data) {
                free(chunk->water_model.data);
                chunk->water_model.data = NULL;
            }
            if (chunk->water_model.triangles) {
                free(chunk->water_model.triangles);
                chunk->water_model.triangles = NULL;
            }
            vbo_free(&chunk->water_model.cache.vbo);
            vao_free(&chunk->water_model.cache.vao);
            ebo_free(&chunk->water_model.cache.ebo);
        }
    }

    free(world->chunks_map);
    free(world->positions_map);
    free(world->index_map);
    free(world->pending_block_changes);

    world->chunks_map = NULL;
    world->positions_map = NULL;
    world->index_map = NULL;
    world->pending_block_changes = NULL;

    override_clear_all();
}