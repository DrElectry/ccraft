#include "core/light.h"
#include "core/tile.h"
#include "core/game.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define LIGHT_LEVELS 16
#define MAX_LIGHT_BYTE 255

#define PAD_W (3 * CHUNK_WIDTH)
#define PAD_D (3 * CHUNK_DEPTH)
#define PAD_H (CHUNK_HEIGHT)
#define PAD_CELLS (PAD_W * PAD_H * PAD_D)
#define PAD_PLANE (PAD_W * PAD_H)

static inline int idx3(int x, int y, int z) {
    return x + CHUNK_WIDTH * (y + CHUNK_HEIGHT * z);
}

static inline uint16_t get_block(const ChunkNeighbors* n, int wx, int wy, int wz) {
    return (uint16_t)chunk_neighbors_get(n, wx, wy, wz);
}

static inline int is_transparent_light(uint16_t id) {
    if (id == AIR) return 1;
    if (id == GLASS) return 1;
    if (id == ROSE) return 1;
    if (id == GRASS_CROSS) return 1;
    if (id == WATER) return 1;
    return 0;
}

static inline int is_light_source(uint16_t id) {
    return id == LAVA;
}

static const uint8_t LIGHT_BYTE_LUT[LIGHT_LEVELS + 1] = {
    0, 15, 31, 47, 63, 79, 95, 111, 127,
    143, 159, 175, 191, 207, 223, 239, 255
};

static inline uint8_t light_to_byte(int lv) {
    if (lv <= 0) return 0;
    if (lv >= LIGHT_LEVELS) return MAX_LIGHT_BYTE;
    return LIGHT_BYTE_LUT[lv];
}

void chunk_light_alloc(Chunk* chunk) {
    if (!chunk) return;
    if (!chunk->sky_light) {
        chunk->sky_light = (uint8_t*)calloc(CHUNK_BLOCK_COUNT, 1);
    }
    if (!chunk->block_light) {
        chunk->block_light = (uint8_t*)calloc(CHUNK_BLOCK_COUNT, 1);
    }
}

void chunk_light_free(Chunk* chunk) {
    if (!chunk) return;
    if (chunk->sky_light) {
        free(chunk->sky_light);
        chunk->sky_light = NULL;
    }
    if (chunk->block_light) {
        free(chunk->block_light);
        chunk->block_light = NULL;
    }
}

typedef struct {
    int x, y, z;
} Pos3; // shit

#if defined(_MSC_VER)
#define LIGHT_TLS __declspec(thread)
#elif defined(__GNUC__) || defined(__clang__)
#define LIGHT_TLS __thread
#else
#define LIGHT_TLS
#endif

static LIGHT_TLS uint8_t pad_work[PAD_CELLS];
static LIGHT_TLS uint8_t pad_flags[PAD_CELLS];
static LIGHT_TLS uint32_t pad_queue[PAD_CELLS];
static LIGHT_TLS int pad_first[PAD_W * PAD_D];

static uint32_t capture_light_flags(const ChunkNeighbors* n, int cx, int cz) {
    int min_wx = (cx - 1) * CHUNK_WIDTH;
    int min_wz = (cz - 1) * CHUNK_DEPTH;
    uint32_t sources = 0;
    for (int oz = 0; oz < PAD_D; oz++) {
        int wz = min_wz + oz;
        int zbase = oz * PAD_PLANE;
        for (int ox = 0; ox < PAD_W; ox++) {
            pad_first[oz * PAD_W + ox] = -1;
        }
        for (int wy = 0; wy < PAD_H; wy++) {
            int base = zbase + wy * PAD_W;
            for (int ox = 0; ox < PAD_W; ox++) {
                int idx = base + ox;
                uint16_t id = get_block(n, min_wx + ox, wy, wz);
                int transparent = is_transparent_light(id);
                pad_flags[idx] = (uint8_t)transparent;
                if (!transparent) {
                    pad_first[oz * PAD_W + ox] = wy;
                }
                if (is_light_source(id)) {
                    pad_queue[sources++] = (uint32_t)idx;
                }
            }
        }
    }
    return sources;
}

static void flood_light(uint32_t q_tail) {
    uint32_t q_head = 0;
    while (q_head < q_tail) {
        uint32_t idx = pad_queue[q_head++];
        uint8_t level = pad_work[idx];
        if (level <= 1) continue;
        uint8_t next = (uint8_t)(level - 1);
        uint32_t x = idx % PAD_W;
        uint32_t yz = idx / PAD_W;
        uint32_t y = yz % PAD_H;
        uint32_t z = yz / PAD_H;
#define VISIT(ni) do {                                     \
    uint32_t at = (uint32_t)(ni);                         \
    if (next > pad_work[at]) {                            \
        pad_work[at] = next;                              \
        if (pad_flags[at]) pad_queue[q_tail++] = at;       \
    }                                                     \
} while (0)
        if (x + 1 < PAD_W) VISIT(idx + 1);
        if (x > 0) VISIT(idx - 1);
        if (y + 1 < PAD_H) VISIT(idx + PAD_W);
        if (y > 0) VISIT(idx - PAD_W);
        if (z + 1 < PAD_D) VISIT(idx + PAD_PLANE);
        if (z > 0) VISIT(idx - PAD_PLANE);
#undef VISIT
    }
}

static void copy_center(uint8_t* out) {
    for (int lz = 0; lz < CHUNK_DEPTH; lz++) {
        int src_z = (lz + CHUNK_DEPTH) * PAD_PLANE;
        int dst_z = lz * CHUNK_WIDTH * CHUNK_HEIGHT;
        for (int wy = 0; wy < CHUNK_HEIGHT; wy++) {
            int src = src_z + wy * PAD_W + CHUNK_WIDTH;
            int dst = dst_z + wy * CHUNK_WIDTH;
            for (int lx = 0; lx < CHUNK_WIDTH; lx++) {
                out[dst + lx] = light_to_byte(pad_work[src + lx]);
            }
        }
    }
}

static void compute_block_light_full(uint8_t* block_out, uint32_t sources) {
    if (sources == 0) {
        memset(block_out, 0, CHUNK_BLOCK_COUNT);
        return;
    }
    memset(pad_work, 0, PAD_CELLS);
    for (uint32_t i = 0; i < sources; i++) {
        pad_work[pad_queue[i]] = LIGHT_LEVELS;
    }
    flood_light(sources);
    copy_center(block_out);
}

static void compute_sky_light_full(uint8_t* sky_out) {
    memset(pad_work, 0, PAD_CELLS);
    // vertical pass: full sky (15) from world top down through transparent blocks.
    // a single top seed + BFS cannot reach the surface (15 block limit
    // AND STILL NO ambient occlusion
    for (int oz = 0; oz < PAD_D; oz++) {
        int zbase = oz * PAD_PLANE;
        for (int ox = 0; ox < PAD_W; ox++) {
            int first = pad_first[oz * PAD_W + ox];
            int bottom = first < 0 ? 0 : first;
            for (int wy = bottom; wy < PAD_H; wy++) {
                pad_work[zbase + wy * PAD_W + ox] = LIGHT_LEVELS;
            }
        }
    }
    uint32_t q_tail = 0;
#define SEED(ni) do {                                     \
    uint32_t at = (uint32_t)(ni);                         \
    if (pad_work[at] == 0) {                             \
        pad_work[at] = LIGHT_LEVELS - 1;                 \
        if (pad_flags[at]) pad_queue[q_tail++] = at;     \
    }                                                    \
} while (0)
    for (int oz = 0; oz < PAD_D; oz++) {
        int zbase = oz * PAD_PLANE;
        for (int ox = 0; ox < PAD_W; ox++) {
            int c = oz * PAD_W + ox;
            int first = pad_first[c];
            int bottom = first < 0 ? 0 : first;
            if (first > 0) {
                SEED(zbase + (first - 1) * PAD_W + ox);
            }
#define SEED_COLUMN(height, delta) do {                    \
    int limit = (height);                                 \
    for (int wy = bottom; wy < limit; wy++) {             \
        SEED(zbase + wy * PAD_W + ox + (delta));           \
    }                                                     \
} while (0)
            if (ox > 0) {
                int h = pad_first[c - 1];
                SEED_COLUMN(h < 0 ? 0 : h, -1);
            }
            if (ox + 1 < PAD_W) {
                int h = pad_first[c + 1];
                SEED_COLUMN(h < 0 ? 0 : h, 1);
            }
            if (oz > 0) {
                int h = pad_first[c - PAD_W];
                SEED_COLUMN(h < 0 ? 0 : h, -PAD_PLANE);
            }
            if (oz + 1 < PAD_D) {
                int h = pad_first[c + PAD_W];
                SEED_COLUMN(h < 0 ? 0 : h, PAD_PLANE);
            }
#undef SEED_COLUMN
        }
    }
#undef SEED
    flood_light(q_tail);
    copy_center(sky_out);
}

void chunk_light_compute_padded(uint8_t* sky_pad, uint8_t* block_pad,
                                uint8_t* sky_out, uint8_t* block_out,
                                const ChunkNeighbors* n, int cx, int cz) {
    (void)sky_pad;
    (void)block_pad;
    uint32_t sources = capture_light_flags(n, cx, cz);
    compute_block_light_full(block_out, sources);
    compute_sky_light_full(sky_out);
}

float chunk_light_vertex(const uint8_t* sky_light, const uint8_t* block_light,
                         int lx, int ly, int lz) {
    int i = idx3(lx, ly, lz);
    uint8_t s = sky_light ? sky_light[i] : 0;
    uint8_t b = block_light ? block_light[i] : 0;
    uint8_t v = s > b ? s : b;
    return (float)v / 255.0f;
}

float chunk_light_face_vertex(const uint8_t* sky_light, const uint8_t* block_light,
                              int lx, int ly, int lz, enum Tile_face face) {
    int sx = lx;
    int sy = ly;
    int sz = lz;
    int offset = 0;

    switch (face) {
    case FRONT:  sz = lz + 1; break; // +Z
    case BACK:   sz = lz - 1; offset = 1; break; // -Z
    case RIGHT:  sx = lx + 1; offset = 1; break; // +X
    case LEFT:   sx = lx - 1; offset = 1; break; // -X
    case UP:     sy = ly + 1; break; // +Y
    case DOWN:   sy = ly - 1; offset = 1; break; // -Y
    }

    if (sx < 0) sx = 0;
    if (sx >= CHUNK_WIDTH) sx = CHUNK_WIDTH - 1;
    if (sy < 0) sy = 0;
    if (sy >= CHUNK_HEIGHT) sy = CHUNK_HEIGHT - 1;
    if (sz < 0) sz = 0;
    if (sz >= CHUNK_DEPTH) sz = CHUNK_DEPTH - 1;

    float light = chunk_light_vertex(sky_light, block_light, sx, sy, sz);
    if (offset && potato_mode) {
        light = fminf(light - 0.4f, 1.0f);
    }
    return light;
}