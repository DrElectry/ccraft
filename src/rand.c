#include "rand.h"

RNG global_rng;

void rng_seed(uint64_t seed)
{
    if (seed == 0)
        seed = 0x9e3779b97f4a7c15ULL;

    global_rng.state = seed;
}

uint64_t rng_next()
{
    uint64_t x = global_rng.state;

    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;

    global_rng.state = x;

    return x * 2685821657736338717ULL;
}

int rng_int(int min, int max)
{
    uint64_t v = rng_next();
    return min + (int)(v % (uint64_t)(max - min + 1));
}

float rng_float()
{
    uint64_t v = rng_next();
    return (v >> 11) * (1.0f / 9007199254740992.0f);
}