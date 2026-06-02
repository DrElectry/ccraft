#ifndef WORLD_GEN_H
#define WORLD_GEN_H

#include <stdint.h>

void world_gen_start(void);
void world_gen_stop(void);

int world_gen_submit(int cx, int cz);
int world_gen_in_flight(int cx, int cz);
int world_gen_poll(int* cx, int* cz, uint16_t** data);

#endif
