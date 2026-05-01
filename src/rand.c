#include "rand.h"

RNG global_rng;

#define WORLD_SEED 0xdeadbeefcafebabeULL

void rng_seed(uint64_t seed) {
    if (seed == 0)
        seed = 0x9e3779b97f4a7c15ULL;
    global_rng.state = seed;
}

uint64_t rng_next() {
    uint64_t x = global_rng.state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    global_rng.state = x;
    return x * 2685821657736338717ULL;
}

int rng_int(int min, int max) {
    return min + (int)(rng_next() % (uint64_t)(max - min + 1));
}

float rng_float() {
    return (rng_next() >> 11) * (1.0f / 9007199254740992.0f);
}

void rng_seed_chunk(int cx, int cz) {
    uint64_t combined = WORLD_SEED;
    combined ^= (uint64_t)(cx * 123456789) + (uint64_t)(cz * 987654321);
    combined ^= (combined >> 33) * 0x9e3779b97f4a7c15ULL;
    rng_seed(combined);
}
