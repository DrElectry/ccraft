#ifndef WORLD_GEN_H
#define WORLD_GEN_H

#include "core/chunk.h"
#include <stdint.h>

struct World;

void world_gen_start(void);
void world_gen_stop(void);

int world_gen_submit(int cx, int cz);
int world_gen_in_flight(int cx, int cz);
int world_gen_poll(int* cx, int* cz, uint16_t** data);

int world_mesh_submit(struct World* world, int cx, int cz);
int world_mesh_in_flight(int cx, int cz);
int world_mesh_poll(int* cx, int* cz, ChunkMeshResult** mesh, uint32_t* generation);

#endif
