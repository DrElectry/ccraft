#include "core/chunk_light.h"
#include "core/tile.h"
#include <stdlib.h>
#include <string.h>

#define LIGHT_LEVELS 16
#define MAX_LIGHT_BYTE 255

static inline int idx3(int x, int y, int z) {
    return x + CHUNK_WIDTH * (y + CHUNK_HEIGHT * z);
}

static inline uint16_t get_block(const ChunkNeighbors* n, int wx, int wy, int wz) {
    return (uint16_t)chunk_neighbors_get(n, wx, wy, wz);
}

static inline int is_transparent_light(uint16_t id) {
    if (id == AIR) return 1;
    if (id == LEAVES) return 1;
    if (id == GLASS) return 1;
    if (id == ROSE) return 1;
    if (id == GRASS_CROSS) return 1;
    return 0;
}

static inline int is_light_source(uint16_t id) {
    return id == LAVA;
}

static inline uint8_t light_to_byte(int lv) {
    if (lv <= 0) return 0;
    if (lv >= LIGHT_LEVELS) return MAX_LIGHT_BYTE;
    return (uint8_t)((lv * (int)MAX_LIGHT_BYTE) / LIGHT_LEVELS);
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
} Pos3;

static void compute_sky_light_full(uint8_t* sky_out, const ChunkNeighbors* n, int cx, int cz) {
    int min_wx = (cx - 1) * CHUNK_WIDTH;
    int max_wx = (cx + 2) * CHUNK_WIDTH - 1;
    int min_wz = (cz - 1) * CHUNK_DEPTH;
    int max_wz = (cz + 2) * CHUNK_DEPTH - 1;
    int width = 3 * CHUNK_WIDTH;
    int depth = 3 * CHUNK_DEPTH;
    int total_blocks = width * CHUNK_HEIGHT * depth;

    uint8_t* work = (uint8_t*)calloc(total_blocks, 1);
    Pos3* queue = (Pos3*)malloc(total_blocks * sizeof(Pos3));
    int q_head = 0, q_tail = 0;

    /* Vertical pass: full sky (15) from world top down through transparent blocks.
     * A single top seed + BFS cannot reach the surface (15-block limit). */
    for (int wx = min_wx; wx <= max_wx; wx++) {
        for (int wz = min_wz; wz <= max_wz; wz++) {
            int column_open = 1;
            for (int wy = CHUNK_HEIGHT - 1; wy >= 0; wy--) {
                if (!column_open)
                    break;

                uint16_t id = get_block(n, wx, wy, wz);
                int ox = wx - min_wx;
                int oz = wz - min_wz;
                int idx = ox + width * (wy + CHUNK_HEIGHT * oz);

                if (is_transparent_light(id)) {
                    if (work[idx] < LIGHT_LEVELS) {
                        work[idx] = LIGHT_LEVELS;
                        queue[q_tail].x = wx;
                        queue[q_tail].y = wy;
                        queue[q_tail].z = wz;
                        q_tail++;
                    }
                } else {
                    if (work[idx] < LIGHT_LEVELS) {
                        work[idx] = LIGHT_LEVELS;
                        queue[q_tail].x = wx;
                        queue[q_tail].y = wy;
                        queue[q_tail].z = wz;
                        q_tail++;
                    }
                    column_open = 0;
                }
            }
        }
    }

    const int dirs[6][3] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    while (q_head < q_tail) {
        int wx = queue[q_head].x;
        int wy = queue[q_head].y;
        int wz = queue[q_head].z;
        q_head++;

        int ox = wx - min_wx;
        int oz = wz - min_wz;
        int cur_idx = ox + width * (wy + CHUNK_HEIGHT * oz);
        int cur_lv = work[cur_idx];
        if (cur_lv <= 1) continue;

        for (int di = 0; di < 6; di++) {
            int nx = wx + dirs[di][0];
            int ny = wy + dirs[di][1];
            int nz = wz + dirs[di][2];
            if (nx < min_wx || nx > max_wx || ny < 0 || ny >= CHUNK_HEIGHT || nz < min_wz || nz > max_wz)
                continue;
            uint16_t nid = get_block(n, nx, ny, nz);

            int nox = nx - min_wx;
            int noz = nz - min_wz;
            int nidx = nox + width * (ny + CHUNK_HEIGHT * noz);
            int next_lv = cur_lv - 1;
            if (next_lv <= 0) continue;
            if (next_lv > work[nidx]) {
                work[nidx] = (uint8_t)next_lv;
                if (is_transparent_light(nid)) {
                    queue[q_tail].x = nx;
                    queue[q_tail].y = ny;
                    queue[q_tail].z = nz;
                    q_tail++;
                }
            }
        }
    }

    int center_min_wx = cx * CHUNK_WIDTH;
    int center_max_wx = center_min_wx + CHUNK_WIDTH - 1;
    int center_min_wz = cz * CHUNK_DEPTH;
    int center_max_wz = center_min_wz + CHUNK_DEPTH - 1;
    for (int wx = center_min_wx; wx <= center_max_wx; wx++) {
        for (int wz = center_min_wz; wz <= center_max_wz; wz++) {
            for (int wy = 0; wy < CHUNK_HEIGHT; wy++) {
                int ox = wx - min_wx;
                int oz = wz - min_wz;
                int idx = ox + width * (wy + CHUNK_HEIGHT * oz);
                uint8_t lv = work[idx];
                int lx = wx - center_min_wx;
                int lz = wz - center_min_wz;
                int out_idx = lx + CHUNK_WIDTH * (wy + CHUNK_HEIGHT * lz);
                sky_out[out_idx] = light_to_byte(lv);
            }
        }
    }

    free(work);
    free(queue);
}

static void compute_block_light_full(uint8_t* block_out, const ChunkNeighbors* n, int cx, int cz) {
    int min_wx = (cx - 1) * CHUNK_WIDTH;
    int max_wx = (cx + 2) * CHUNK_WIDTH - 1;
    int min_wz = (cz - 1) * CHUNK_DEPTH;
    int max_wz = (cz + 2) * CHUNK_DEPTH - 1;
    int width = 3 * CHUNK_WIDTH;
    int depth = 3 * CHUNK_DEPTH;
    int total_blocks = width * CHUNK_HEIGHT * depth;

    uint8_t* work = (uint8_t*)calloc(total_blocks, 1);
    Pos3* queue = (Pos3*)malloc(total_blocks * sizeof(Pos3));
    int q_head = 0, q_tail = 0;

    for (int wx = min_wx; wx <= max_wx; wx++) {
        for (int wz = min_wz; wz <= max_wz; wz++) {
            for (int wy = 0; wy < CHUNK_HEIGHT; wy++) {
                uint16_t id = get_block(n, wx, wy, wz);
                if (is_light_source(id)) {
                    int ox = wx - min_wx;
                    int oz = wz - min_wz;
                    int idx = ox + width * (wy + CHUNK_HEIGHT * oz);
                    if (work[idx] == 0) {
                        work[idx] = LIGHT_LEVELS;
                        queue[q_tail].x = wx;
                        queue[q_tail].y = wy;
                        queue[q_tail].z = wz;
                        q_tail++;
                    }
                }
            }
        }
    }

    const int dirs[6][3] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    while (q_head < q_tail) {
        int wx = queue[q_head].x;
        int wy = queue[q_head].y;
        int wz = queue[q_head].z;
        q_head++;

        int ox = wx - min_wx;
        int oz = wz - min_wz;
        int cur_idx = ox + width * (wy + CHUNK_HEIGHT * oz);
        int cur_lv = work[cur_idx];
        if (cur_lv <= 1) continue;

        for (int di = 0; di < 6; di++) {
            int nx = wx + dirs[di][0];
            int ny = wy + dirs[di][1];
            int nz = wz + dirs[di][2];
            if (nx < min_wx || nx > max_wx || ny < 0 || ny >= CHUNK_HEIGHT || nz < min_wz || nz > max_wz)
                continue;
            uint16_t nid = get_block(n, nx, ny, nz);

            int nox = nx - min_wx;
            int noz = nz - min_wz;
            int nidx = nox + width * (ny + CHUNK_HEIGHT * noz);
            int next_lv = cur_lv - 1;
            if (next_lv <= 0) continue;
            if (next_lv > work[nidx]) {
                work[nidx] = (uint8_t)next_lv;
                if (is_transparent_light(nid)) {
                    queue[q_tail].x = nx;
                    queue[q_tail].y = ny;
                    queue[q_tail].z = nz;
                    q_tail++;
                }
            }
        }
    }

    int center_min_wx = cx * CHUNK_WIDTH;
    int center_max_wx = center_min_wx + CHUNK_WIDTH - 1;
    int center_min_wz = cz * CHUNK_DEPTH;
    int center_max_wz = center_min_wz + CHUNK_DEPTH - 1;
    for (int wx = center_min_wx; wx <= center_max_wx; wx++) {
        for (int wz = center_min_wz; wz <= center_max_wz; wz++) {
            for (int wy = 0; wy < CHUNK_HEIGHT; wy++) {
                int ox = wx - min_wx;
                int oz = wz - min_wz;
                int idx = ox + width * (wy + CHUNK_HEIGHT * oz);
                uint8_t lv = work[idx];
                int lx = wx - center_min_wx;
                int lz = wz - center_min_wz;
                int out_idx = lx + CHUNK_WIDTH * (wy + CHUNK_HEIGHT * lz);
                block_out[out_idx] = light_to_byte(lv);
            }
        }
    }

    free(work);
    free(queue);
}

void chunk_light_compute_padded(uint8_t* sky_pad, uint8_t* block_pad,
                                 uint8_t* sky_out, uint8_t* block_out,
                                 const ChunkNeighbors* n, int cx, int cz) {
    (void)sky_pad;
    (void)block_pad;
    compute_sky_light_full(sky_out, n, cx, cz);
    compute_block_light_full(block_out, n, cx, cz);
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
    (void)face;
    return chunk_light_vertex(sky_light, block_light, lx, ly, lz);
}