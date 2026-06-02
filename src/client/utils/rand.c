#include "rand.h"
#include <stdio.h>

RNG global_rng;

// simple xorshift64 *random* numbers generator

uint64_t world_seed = 0xdeadbeefcafebabeULL;

uint64_t rng_get_world_seed(void) {
    return world_seed;
}

void rng_seed(uint64_t seed) {
    world_seed = seed;

    if (seed == 0)
        seed = 0x9e3779b97f4a7c15ULL;
    global_rng.state = seed;
}

static uint64_t rng_step(uint64_t* state) {
    uint64_t x = *state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    *state = x;
    return x * 2685821657736338717ULL;
}

uint64_t rng_next() {
    return rng_step(&global_rng.state);
}

uint64_t rng_next_r(RNG* rng) {
    return rng_step(&rng->state);
}

int rng_int(int min, int max) {
    return min + (int)(rng_next() % (uint64_t)(max - min + 1));
}

int rng_int_r(RNG* rng, int min, int max) {
    return min + (int)(rng_next_r(rng) % (uint64_t)(max - min + 1));
}

float rng_float() {
    return (rng_next() >> 11) * (1.0f / 9007199254740992.0f);
}

float rng_float_r(RNG* rng) {
    return (rng_next_r(rng) >> 11) * (1.0f / 9007199254740992.0f);
}

static void rng_seed_chunk_state(uint64_t* state, int cx, int cz) {
    uint64_t combined = world_seed;
    combined ^= (uint64_t)(cx * 123456789) + (uint64_t)(cz * 987654321);
    combined ^= (combined >> 33) * 0x9e3779b97f4a7c15ULL;
    *state = combined;
}

void rng_seed_chunk(int cx, int cz) {
    rng_seed_chunk_state(&global_rng.state, cx, cz);
}

void rng_seed_chunk_r(RNG* rng, int cx, int cz) {
    rng_seed_chunk_state(&rng->state, cx, cz);
}