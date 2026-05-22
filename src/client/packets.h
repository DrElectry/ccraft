#ifndef PACKETS_H
#define PACKETS_H

#include <stdint.h>

// PACKET SEED IS TEMPORAL !!!!!!!

enum {
    PKT_SEED = 1
};

#pragma pack(push, 1)

typedef struct {
    uint8_t type; // for the future
    uint64_t seed;
} Seedpckt;

#pragma pack(pop)

#endif