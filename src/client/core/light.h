#ifndef LIGHT_H
#define LIGHT_H

#include "core/chunk.h"

#include "core/tile.h"
#include <stdint.h>

#define CHUNK_LIGHT_PAD_COUNT (CHUNK_BLOCK_COUNT)


void chunk_light_alloc(Chunk* chunk);
void chunk_light_free(Chunk* chunk);


void chunk_light_compute_padded(uint8_t* sky_pad, uint8_t* block_pad,

                                 uint8_t* sky_out, uint8_t* block_out,
                                 const ChunkNeighbors* n, int cx, int cz);

float chunk_light_vertex(const uint8_t* sky_light, const uint8_t* block_light,
                          int lx, int ly, int lz);

float chunk_light_face_vertex(const uint8_t* sky_light, const uint8_t* block_light,
                               int lx, int ly, int lz, enum Tile_face face);

#endif




