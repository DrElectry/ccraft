#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "globals.h"
#include "net_platform.h"

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <arpa/inet.h>
    #include <netinet/tcp.h>
#endif

Server global_server;   

typedef struct {
    int32_t x;
    int32_t y;
    int32_t z;
    uint16_t block_type;
} ServerBlockChange;

static ServerBlockChange* g_pending_block_changes = NULL;
static int g_pending_block_count = 0;
static int g_pending_block_capacity = 0;
static pthread_mutex_t g_block_mutex = PTHREAD_MUTEX_INITIALIZER;

static void server_add_block_change(int32_t x, int32_t y, int32_t z, uint16_t block_type) {
    pthread_mutex_lock(&g_block_mutex);
    
    for (int i = 0; i < g_pending_block_count; i++) {
        if (g_pending_block_changes[i].x == x && 
            g_pending_block_changes[i].y == y &&
            g_pending_block_changes[i].z == z) {
            g_pending_block_changes[i].block_type = block_type;
            pthread_mutex_unlock(&g_block_mutex);
            return;
        }
    }
    
    if (g_pending_block_count >= g_pending_block_capacity) {
        int new_capacity = g_pending_block_capacity == 0 ? 64000 : g_pending_block_capacity * 2;
        ServerBlockChange* new_changes = realloc(g_pending_block_changes, sizeof(ServerBlockChange) * new_capacity);
        if (new_changes) {
            g_pending_block_changes = new_changes;
            g_pending_block_capacity = new_capacity;
        } else {
            pthread_mutex_unlock(&g_block_mutex);
            return;
        }
    }
    
    g_pending_block_changes[g_pending_block_count].x = x;
    g_pending_block_changes[g_pending_block_count].y = y;
    g_pending_block_changes[g_pending_block_count].z = z;
    g_pending_block_changes[g_pending_block_count].block_type = block_type;
    g_pending_block_count++;
    
    pthread_mutex_unlock(&g_block_mutex);
}

static void send_world_snapshot(Client* client) {
    pthread_mutex_lock(&g_block_mutex);
    
    if (g_pending_block_count == 0) {
        pthread_mutex_unlock(&g_block_mutex);
        return;
    }
    
    size_t data_size = sizeof(WorldSnapshotPacket) + g_pending_block_count * sizeof(BlockChangeData);
    
    uint8_t* buffer = malloc(data_size);
    if (!buffer) {
        pthread_mutex_unlock(&g_block_mutex);
        return;
    }
    
    WorldSnapshotPacket* header = (WorldSnapshotPacket*)buffer;
    header->type = PKT_WORLD_SNAPSHOT;
    header->count = g_pending_block_count;
    
    BlockChangeData* blocks = (BlockChangeData*)(buffer + sizeof(WorldSnapshotPacket));
    for (int i = 0; i < g_pending_block_count; i++) {
        blocks[i].x = g_pending_block_changes[i].x;
        blocks[i].y = g_pending_block_changes[i].y;
        blocks[i].z = g_pending_block_changes[i].z;
        blocks[i].block_type = g_pending_block_changes[i].block_type;
    }
    
    send(client->socket, (const char*)buffer, (int)data_size, 0);
    free(buffer);
    
    printf("Sent world snapshot with %d block changes to client %u\n", 
           g_pending_block_count, client->client_id);
    
    pthread_mutex_unlock(&g_block_mutex);
}

void* handle_client(void* arg) {
    Client* client = (Client*)arg;
    uint8_t buffer[4096];
    
    HandshakePacket handshake_recv;
    memset(&handshake_recv, 0, sizeof(handshake_recv));
    
    size_t got = 0;
    while (got < sizeof(HandshakePacket)) {
        int bytes = recv(client->socket, ((uint8_t*)&handshake_recv) + got, 
                        sizeof(HandshakePacket) - got, 0);
        if (bytes > 0) {
            got += bytes;
        } else if (bytes == 0 || (bytes < 0 && net_get_error() != NET_EAGAIN && net_get_error() != NET_EWOULDBLOCK)) {
            net_close(client->socket);
            return NULL;
        }
    }
    
    strncpy(client->nickname, handshake_recv.nickname, MAX_NICKNAME_LEN - 1);
    client->nickname[MAX_NICKNAME_LEN - 1] = '\0';
    
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char client_ip[INET_ADDRSTRLEN];
    if (getpeername(client->socket, (struct sockaddr*)&client_addr, &addr_len) == 0) {
        inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
    } else {
        strcpy(client_ip, "unknown");
    }
    
    printf("%s joined as <%s>\n", client_ip, client->nickname);
    
    HandshakePacket handshake;
    handshake.type = PKT_HANDSHAKE;
    handshake.client_id = client->client_id;
    handshake.seed = WORLD_SEED;
    strncpy(handshake.nickname, client->nickname, MAX_NICKNAME_LEN);
    if (send(client->socket, (const char*)&handshake, (int)sizeof(handshake), 0) < 0) {
        perror("send handshake");
        net_close(client->socket);
        return NULL;
    }
    
    PlayerSpawnPacket spawn;
    spawn.type = PKT_PLAYER_SPAWN;
    spawn.client_id = client->client_id;
    memcpy(spawn.pos, client->pos, sizeof(spawn.pos));
    memcpy(spawn.rot, client->rot, sizeof(spawn.rot));
    strncpy(spawn.nickname, client->nickname, MAX_NICKNAME_LEN);
    
    pthread_mutex_lock(&global_server.clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (global_server.clients[i].active && global_server.clients[i].client_id != client->client_id) {
            send(global_server.clients[i].socket, (const char*)&spawn, (int)sizeof(spawn), 0);
            PlayerSpawnPacket existing;
            existing.type = PKT_PLAYER_SPAWN;
            existing.client_id = global_server.clients[i].client_id;
            memcpy(existing.pos, global_server.clients[i].pos, sizeof(existing.pos));
            memcpy(existing.rot, global_server.clients[i].rot, sizeof(existing.rot));
            strncpy(existing.nickname, global_server.clients[i].nickname, MAX_NICKNAME_LEN);
            send(client->socket, (const char*)&existing, (int)sizeof(existing), 0);
        }
    }
    pthread_mutex_unlock(&global_server.clients_mutex);
    
    send_world_snapshot(client);
    
    
    int send_buf_size = 256 * 1024;
    int recv_buf_size = 256 * 1024;
    setsockopt(client->socket, SOL_SOCKET, SO_SNDBUF, (const char*)&send_buf_size, sizeof(send_buf_size));
    setsockopt(client->socket, SOL_SOCKET, SO_RCVBUF, (const char*)&recv_buf_size, sizeof(recv_buf_size));
    
    while (client->active) {
        int bytes = recv(client->socket, buffer, sizeof(buffer), 0);
        
        if (bytes > 0) {
            int offset = 0;
            while (offset < bytes) {
                uint8_t type = buffer[offset];
                
                if (type == PKT_PLAYER_STATE) {
                    if (offset + sizeof(PlayerStatePacket) > bytes) {
                        break;
                    }
                    PlayerStatePacket* state = (PlayerStatePacket*)(buffer + offset);
                    memcpy(client->pos, state->pos, sizeof(client->pos));
                    memcpy(client->rot, state->rot, sizeof(client->rot));
                    client->on_ground = state->on_ground;
                    
                    if (strlen(state->nickname) > 0) {
                        strncpy(client->nickname, state->nickname, MAX_NICKNAME_LEN);
                    }

                    pthread_mutex_lock(&global_server.clients_mutex);
                    for (int i = 0; i < MAX_CLIENTS; i++) {
                        if (global_server.clients[i].active && global_server.clients[i].client_id != client->client_id) {
                            if (send(global_server.clients[i].socket, (const char*)state, (int)sizeof(PlayerStatePacket), 0) < 0) {
                                int err = net_get_error();
                                if (err != NET_EAGAIN && err != NET_EWOULDBLOCK) {
                                    global_server.clients[i].active = 0;
                                    net_close(global_server.clients[i].socket);
                                }
                            }
                        }
                    }
                    pthread_mutex_unlock(&global_server.clients_mutex);
                    offset += sizeof(PlayerStatePacket);
                }
                else if (type == PKT_BLOCK_UPDATE) {
                    if (offset + sizeof(BlockUpdatePacket) > bytes) {
                        break;
                    }
                    BlockUpdatePacket* block = (BlockUpdatePacket*)(buffer + offset);
                    
                    server_add_block_change(block->x, block->y, block->z, block->block_type);
                    
                    pthread_mutex_lock(&global_server.clients_mutex);
                    for (int i = 0; i < MAX_CLIENTS; i++) {
                        if (global_server.clients[i].active) {
                            if (send(global_server.clients[i].socket, (const char*)block, (int)sizeof(BlockUpdatePacket), 0) < 0) {
                                int err = net_get_error();
                                if (err != NET_EAGAIN && err != NET_EWOULDBLOCK) {
                                    global_server.clients[i].active = 0;
                                    net_close(global_server.clients[i].socket);
                                }
                            }
                        }
                    }
                    pthread_mutex_unlock(&global_server.clients_mutex);
                    offset += sizeof(BlockUpdatePacket);
                }
                else {
                    printf("Unknown packet type %d from client %u\n", type, client->client_id);
                    offset++;
                }
            }
        }
        else if (bytes == 0) {
            break;
        }
        else if (bytes < 0) {
            int err = net_get_error();
            if (err != NET_EAGAIN && err != NET_EWOULDBLOCK) {
                break;
            }
        }
    }
    
    client->active = 0;
    net_close(client->socket);
    
    char disconnect_nickname[MAX_NICKNAME_LEN];
    strncpy(disconnect_nickname, client->nickname, MAX_NICKNAME_LEN);
    
    PlayerDespawnPacket despawn;
    despawn.type = PKT_PLAYER_DESPAWN;
    despawn.client_id = client->client_id;
    
    pthread_mutex_lock(&global_server.clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (global_server.clients[i].active) {
            send(global_server.clients[i].socket, (const char*)&despawn, (int)sizeof(despawn), 0);
        }
    }
    pthread_mutex_unlock(&global_server.clients_mutex);
    
    printf("%s disconnected.\n", disconnect_nickname);
    return NULL;
}

void server_init(Server* server) {
    memset(server->clients, 0, sizeof(server->clients));
    server->running = 0;
    pthread_mutex_init(&server->clients_mutex, NULL);
    
    g_pending_block_changes = NULL;
    g_pending_block_count = 0;
    g_pending_block_capacity = 0;
}

void server_start(Server* server) {
    server->server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server->server_socket == NET_INVALID_SOCKET) {
        perror("socket");
        return;
    }
    
    int opt = 1;
    setsockopt(server->server_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(server->server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == NET_SOCKET_ERROR) {
        perror("bind");
        net_close(server->server_socket);
        return;
    }
    
    if (listen(server->server_socket, MAX_CLIENTS) == NET_SOCKET_ERROR) {
        perror("listen");
        net_close(server->server_socket);
        return;
    }
    
    printf("CCRAFT server (version %s) \n", SERVER_VERSION);
    server->running = 1;
    printf("Server listening on port %d\n", SERVER_PORT);
    
    fd_set read_fds;
    uint32_t next_client_id = 1;
    
    while (server->running) {
        FD_ZERO(&read_fds);
        FD_SET(server->server_socket, &read_fds);
        int max_fd = (int)server->server_socket;
        
        pthread_mutex_lock(&server->clients_mutex);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (server->clients[i].active) {
                FD_SET(server->clients[i].socket, &read_fds);
                if ((int)server->clients[i].socket > max_fd) {
                    max_fd = (int)server->clients[i].socket;
                }
            }
        }
        pthread_mutex_unlock(&server->clients_mutex);
        
        if (net_select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0) {
            break;
        }
        
        if (FD_ISSET(server->server_socket, &read_fds)) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            net_socket_t client_socket = accept(server->server_socket, (struct sockaddr*)&client_addr, &client_len);

            if (client_socket == NET_INVALID_SOCKET) {
                perror("accept");
                continue;
            }

            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
            
            int flag = 1;
#ifdef _WIN32
            setsockopt(client_socket, IPPROTO_TCP, TCP_NODELAY, (const char*)&flag, sizeof(int));
#else
            setsockopt(client_socket, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int));
#endif
            
            int slot = -1;
            pthread_mutex_lock(&server->clients_mutex);
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (!server->clients[i].active) {
                    slot = i;
                    break;
                }
            }
            
            if (slot >= 0) {
                server->clients[slot].socket = client_socket;
                server->clients[slot].client_id = next_client_id++;
                server->clients[slot].active = 1;
                server->clients[slot].on_ground = 0;
                memset(server->clients[slot].nickname, 0, MAX_NICKNAME_LEN);
                memset(server->clients[slot].pos, 0, sizeof(server->clients[slot].pos));
                memset(server->clients[slot].rot, 0, sizeof(server->clients[slot].rot));
                pthread_create(&server->clients[slot].thread, NULL, handle_client, &server->clients[slot]);
            } else {
                printf("Server full, rejecting connection from %s\n", client_ip);
                net_close(client_socket);
            }
            pthread_mutex_unlock(&server->clients_mutex);
        }
    }
}

void server_broadcast(Server* server, const void* data, size_t size, uint32_t exclude_id) {
    pthread_mutex_lock(&server->clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (server->clients[i].active && server->clients[i].client_id != exclude_id) {
            if (send(server->clients[i].socket, (const char*)data, size, 0) < 0) {
                int err = net_get_error();
                if (err != NET_EAGAIN && err != NET_EWOULDBLOCK) {
                    server->clients[i].active = 0;
                    net_close(server->clients[i].socket);
                }
            }
        }
    }
    pthread_mutex_unlock(&server->clients_mutex);
}

void server_broadcast_except(Server* server, const void* data, size_t size, uint32_t exclude_id) {
    server_broadcast(server, data, size, exclude_id);
}

void server_stop(Server* server) {
    server->running = 0;
    pthread_mutex_lock(&server->clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (server->clients[i].active) {
            server->clients[i].active = 0;
            net_close(server->clients[i].socket);
        }
    }
    pthread_mutex_unlock(&server->clients_mutex);
    net_close(server->server_socket);
    pthread_mutex_destroy(&server->clients_mutex);
    
    pthread_mutex_lock(&g_block_mutex);
    if (g_pending_block_changes) {
        free(g_pending_block_changes);
        g_pending_block_changes = NULL;
    }
    pthread_mutex_unlock(&g_block_mutex);
    pthread_mutex_destroy(&g_block_mutex);
}