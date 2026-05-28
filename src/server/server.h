#ifndef SERVER_H
#define SERVER_H

#include <stdint.h>
#include <pthread.h>
#include "packets.h"

#include "globals.h"
#include "net_platform.h"

typedef struct {
    net_socket_t socket;
    pthread_t thread;
    uint32_t client_id;
    char nickname[MAX_NICKNAME_LEN];
    int active;
    float pos[3];
    float rot[3];
    uint8_t on_ground;
} Client;

typedef struct {
    Client clients[MAX_CLIENTS];
    int server_socket;
    volatile int running;
    pthread_mutex_t clients_mutex;
} Server;

extern Server global_server;

void server_init(Server* server);
void server_start(Server* server);
void server_broadcast(Server* server, const void* data, size_t size, uint32_t exclude_id);
void server_broadcast_except(Server* server, const void* data, size_t size, uint32_t exclude_id);
void server_stop(Server* server);

#endif