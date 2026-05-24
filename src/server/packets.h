#ifndef PACKETS_H
#define PACKETS_H

#include <stdint.h>

enum {
    PKT_HANDSHAKE = 1,
    PKT_PLAYER_STATE,
    PKT_PLAYER_SPAWN,
    PKT_PLAYER_DESPAWN,
    PKT_CHUNK_DATA,
    PKT_BLOCK_UPDATE,
    PKT_PING,
    PKT_WORLD_SNAPSHOT
};

#pragma pack(push, 1)

typedef struct {
    uint8_t type;
    uint32_t client_id;
    uint64_t seed;
} HandshakePacket;

typedef struct {
    uint8_t type;
    uint32_t client_id;
    float pos[3];
    float rot[3];
    uint8_t on_ground;
} PlayerStatePacket;

typedef struct {
    uint8_t type;
    uint32_t client_id;
    float pos[3];
    float rot[3];
} PlayerSpawnPacket;

typedef struct {
    uint8_t type;
    uint32_t client_id;
} PlayerDespawnPacket;

typedef struct {
    uint8_t type;
    int32_t x;
    int32_t y;
    int32_t z;
    uint16_t block_type;
} BlockUpdatePacket;

typedef struct {
    uint8_t type;
    uint32_t count;
} WorldSnapshotPacket;

typedef struct {
    int32_t x;
    int32_t y;
    int32_t z;
    uint16_t block_type;
} BlockChangeData;

#pragma pack(pop)

#endif