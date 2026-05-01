#ifndef RAND_H
#define RAND_H

#include <stdint.h>

typedef struct {
    uint64_t state;
} RNG;

void rng_seed(uint64_t seed);
uint64_t rng_next();
void rng_seed_chunk(int cx, int cz);

int rng_int(int min, int max);
float rng_float();

#define RAND(min, max) rng_int((min), (max))
#define RANDF() rng_float()

extern RNG global_rng;

#endif
