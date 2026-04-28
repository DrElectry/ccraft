#include "rand.h"

void rng_seed(RNG *r, uint64_t seed)
{
    if (seed == 0)
        seed = 0x9e3779b97f4a7c15ULL;

    r->state = seed;
}

uint64_t rng_next(RNG *r)
{
    uint64_t x = r->state;

    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;

    r->state = x;

    return x * 2685821657736338717ULL;
}

int rng_int(RNG *r, int min, int max)
{
    uint64_t v = rng_next(r);
    return min + (int)(v % (uint64_t)(max - min + 1));
}

float rng_float(RNG *r)
{
    uint64_t v = rng_next(r);
    return (v >> 11) * (1.0f / 9007199254740992.0f);
}