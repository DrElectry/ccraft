#ifndef CLIENT_NETWORK_H
#define CLIENT_NETWORK_H

#include <stdint.h>

#define CLIENT_MAX_REMOTES 32

#define UPDATE_RATE 64 // same as in src/server/globals.h
#define MAX_NICKNAME_LEN 32

typedef struct {
    uint32_t client_id;
    uint8_t active;
    float pos[3];
    float rot[3];
    uint8_t on_ground;
    char nickname[MAX_NICKNAME_LEN];
} RemotePlayer;

int network_init(int server_port);

int network_connect_and_handshake(const char* host, int port, uint64_t* out_seed, const char*  nickname);

// polls sockets for incoming frame
void network_pump(void);

void network_send_player_state(uint32_t client_id, const float pos[3], const float rot[3], uint8_t on_ground);
void network_send_block_change(uint32_t client_id, int32_t x, int32_t y, int32_t z, uint16_t type);

uint64_t network_get_seed(void);

RemotePlayer* network_get_remote_players(void);

uint32_t network_get_local_client_id(void);

#endif