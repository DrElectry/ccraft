#ifndef RAND_H
#define RAND_H

#include <stdint.h>

typedef struct {
    uint64_t state;
} RNG;

void     rng_seed(RNG *r, uint64_t seed);
uint64_t rng_next(RNG *r);

int   rng_int(RNG *r, int min, int max);
float rng_float(RNG *r);

#define RAND(r, min, max)   rng_int((r), (min), (max))
#define RANDF(r)            rng_float((r))

#endif