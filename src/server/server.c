#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <fcntl.h>
#include <errno.h>
#include <netinet/tcp.h>

#include "globals.h"

Server global_server;

void* handle_client(void* arg) {
    Client* client = (Client*)arg;
    uint8_t buffer[4096];
    
    HandshakePacket handshake;
    handshake.type = PKT_HANDSHAKE;
    handshake.client_id = client->client_id;
    handshake.seed = WORLD_SEED;
    if (send(client->socket, &handshake, sizeof(handshake), 0) < 0) {
        perror("send handshake");
        close(client->socket);
        return NULL;
    }
    
    PlayerSpawnPacket spawn;
    spawn.type = PKT_PLAYER_SPAWN;
    spawn.client_id = client->client_id;
    memcpy(spawn.pos, client->pos, sizeof(spawn.pos));
    memcpy(spawn.rot, client->rot, sizeof(spawn.rot));
    
    pthread_mutex_lock(&global_server.clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (global_server.clients[i].active && global_server.clients[i].client_id != client->client_id) {
            send(global_server.clients[i].socket, &spawn, sizeof(spawn), 0);
            PlayerSpawnPacket existing;
            existing.type = PKT_PLAYER_SPAWN;
            existing.client_id = global_server.clients[i].client_id;
            memcpy(existing.pos, global_server.clients[i].pos, sizeof(existing.pos));
            memcpy(existing.rot, global_server.clients[i].rot, sizeof(existing.rot));
            send(client->socket, &existing, sizeof(existing), 0);
        }
    }
    pthread_mutex_unlock(&global_server.clients_mutex);
    
    int send_buf_size = 256 * 1024;
    int recv_buf_size = 256 * 1024;
    setsockopt(client->socket, SOL_SOCKET, SO_SNDBUF, &send_buf_size, sizeof(send_buf_size));
    setsockopt(client->socket, SOL_SOCKET, SO_RCVBUF, &recv_buf_size, sizeof(recv_buf_size));
    
    while (client->active) {
        int bytes = recv(client->socket, buffer, sizeof(buffer), 0);
        
        if (bytes > 0) {
            // now we are processing all of the packets
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
                    
                    printf("[srv] got state from %u pos=(%f,%f,%f) rot=(%f,%f,%f)\n",
                           client->client_id,
                           state->pos[0], state->pos[1], state->pos[2],
                           state->rot[0], state->rot[1], state->rot[2]);

                    pthread_mutex_lock(&global_server.clients_mutex);
                    for (int i = 0; i < MAX_CLIENTS; i++) {
                        if (global_server.clients[i].active && global_server.clients[i].client_id != client->client_id) {
                            if (send(global_server.clients[i].socket, state, sizeof(PlayerStatePacket), 0) < 0) {
                                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                                    global_server.clients[i].active = 0;
                                    close(global_server.clients[i].socket);
                                }
                            }
                        }
                    }
                    pthread_mutex_unlock(&global_server.clients_mutex);
                    offset += sizeof(PlayerStatePacket);
                }
                else if (type == PKT_BLOCK_UPDATE) {
                    if (offset + sizeof(BlockUpdatePacket) > bytes) {
                        break; // incomplete packet, wait for next recv
                    }
                    BlockUpdatePacket* block = (BlockUpdatePacket*)(buffer + offset);
                    pthread_mutex_lock(&global_server.clients_mutex);
                    for (int i = 0; i < MAX_CLIENTS; i++) {
                        if (global_server.clients[i].active) {
                            if (send(global_server.clients[i].socket, block, sizeof(BlockUpdatePacket), 0) < 0) {
                                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                                    global_server.clients[i].active = 0;
                                    close(global_server.clients[i].socket);
                                }
                            }
                        }
                    }
                    pthread_mutex_unlock(&global_server.clients_mutex);
                    offset += sizeof(BlockUpdatePacket);
                }
                else {
                    printf("[srv] Unknown packet type %d from client %u\n", type, client->client_id);
                    offset++;
                }
            }
        }
        else if (bytes == 0) {
            break;
        }
        else if (bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            break;
        }
    }
    
    client->active = 0;
    close(client->socket);
    
    PlayerDespawnPacket despawn;
    despawn.type = PKT_PLAYER_DESPAWN;
    despawn.client_id = client->client_id;
    
    pthread_mutex_lock(&global_server.clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (global_server.clients[i].active) {
            send(global_server.clients[i].socket, &despawn, sizeof(despawn), 0);
        }
    }
    pthread_mutex_unlock(&global_server.clients_mutex);
    
    printf("Client %u disconnected\n", client->client_id);
    return NULL;
}

void server_init(Server* server) {
    memset(server->clients, 0, sizeof(server->clients));
    server->running = 0;
    pthread_mutex_init(&server->clients_mutex, NULL);
}

void server_start(Server* server) {
    server->server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server->server_socket < 0) {
        perror("socket");
        return;
    }
    
    int opt = 1;
    setsockopt(server->server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(server->server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(server->server_socket);
        return;
    }
    
    if (listen(server->server_socket, MAX_CLIENTS) < 0) {
        perror("listen");
        close(server->server_socket);
        return;
    }
    
    server->running = 1;
    printf("Server listening on port %d\n", SERVER_PORT);
    
    fd_set read_fds;
    uint32_t next_client_id = 1;
    
    while (server->running) {
        FD_ZERO(&read_fds);
        FD_SET(server->server_socket, &read_fds);
        int max_fd = server->server_socket;
        
        pthread_mutex_lock(&server->clients_mutex);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (server->clients[i].active) {
                FD_SET(server->clients[i].socket, &read_fds);
                if (server->clients[i].socket > max_fd) {
                    max_fd = server->clients[i].socket;
                }
            }
        }
        pthread_mutex_unlock(&server->clients_mutex);
        
        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0) {
            break;
        }
        
        if (FD_ISSET(server->server_socket, &read_fds)) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int client_socket = accept(server->server_socket, (struct sockaddr*)&client_addr, &client_len);
            
            if (client_socket < 0) {
                perror("accept");
                continue;
            }
            
            int flag = 1;
            setsockopt(client_socket, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int));
            
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
                memset(server->clients[slot].pos, 0, sizeof(server->clients[slot].pos));
                memset(server->clients[slot].rot, 0, sizeof(server->clients[slot].rot));
                pthread_create(&server->clients[slot].thread, NULL, handle_client, &server->clients[slot]);
                printf("Client %u connected\n", server->clients[slot].client_id);
            } else {
                printf("Server full, rejecting connection\n");
                close(client_socket);
            }
            pthread_mutex_unlock(&server->clients_mutex);
        }
    }
}

void server_broadcast(Server* server, const void* data, size_t size, uint32_t exclude_id) {
    pthread_mutex_lock(&server->clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (server->clients[i].active && server->clients[i].client_id != exclude_id) {
            if (send(server->clients[i].socket, data, size, 0) < 0) {
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    server->clients[i].active = 0;
                    close(server->clients[i].socket);
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
            close(server->clients[i].socket);
        }
    }
    pthread_mutex_unlock(&server->clients_mutex);
    close(server->server_socket);
    pthread_mutex_destroy(&server->clients_mutex);
}