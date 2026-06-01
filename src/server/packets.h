#ifndef PACKETS_H
#define PACKETS_H

#include <stdint.h>

#define MAX_NICKNAME_LEN 32
#define MAX_SERVER_NAME_LEN 64
#define MAX_SERVER_DESC_LEN 512
#define MAX_CHAT_MESSAGE_LEN 256

enum {
    PKT_HANDSHAKE = 1,
    PKT_PLAYER_STATE,
    PKT_PLAYER_SPAWN,
    PKT_PLAYER_DESPAWN,
    PKT_CHUNK_DATA,
    PKT_BLOCK_UPDATE,
    PKT_PING,
    PKT_WORLD_SNAPSHOT,
    PKT_CHAT_MESSAGE
};

#pragma pack(push, 1)

typedef struct {
    uint8_t type;
    uint32_t client_id;
    uint64_t seed;

    char nickname[MAX_NICKNAME_LEN];

    // metadata
    char server_name[MAX_SERVER_NAME_LEN];
    char server_description[MAX_SERVER_DESC_LEN];
} HandshakePacket;

typedef struct {
    uint8_t type;
    uint32_t client_id;
    float pos[3];
    float rot[3];
    uint8_t on_ground;
    char nickname[MAX_NICKNAME_LEN];
} PlayerStatePacket;

typedef struct {
    uint8_t type;
    uint32_t client_id;
    float pos[3];
    float rot[3];
    char nickname[MAX_NICKNAME_LEN];
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

typedef struct {
    uint8_t type;
    uint32_t client_id;
    char nickname[MAX_NICKNAME_LEN];
    char message[MAX_CHAT_MESSAGE_LEN];
} ChatMessagePacket;

#pragma pack(pop)

#endif