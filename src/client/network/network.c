#include "network/network.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "network/packets.h"

#include <stdint.h>

#include "core/world.h"

// a little abstraction layer between raw ass sockets and our prestige ccraft client

static int g_sock = -1;
static uint64_t g_seed = 0;
static uint32_t g_local_client_id = 0;
static RemotePlayer g_remotes[CLIENT_MAX_REMOTES];

// for multiple recv calls
static uint8_t recv_buffer[65536];
static size_t recv_buffer_len = 0;

static void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return;
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int network_init(int server_port) {
    (void)server_port;
    memset(g_remotes, 0, sizeof(g_remotes));
    g_seed = 0;
    g_local_client_id = 0;
    recv_buffer_len = 0;
    return 0;
}

RemotePlayer* network_get_remote_players(void) {
    return g_remotes;
}

uint64_t network_get_seed(void) {
    return g_seed;
}

uint32_t network_get_local_client_id(void) {
    return g_local_client_id;
}

static RemotePlayer* find_remote(uint32_t client_id) {
    for (int i = 0; i < CLIENT_MAX_REMOTES; i++) {
        if (g_remotes[i].active && g_remotes[i].client_id == client_id) {
            return &g_remotes[i];
        }
    }
    return NULL;
}

static RemotePlayer* alloc_remote(uint32_t client_id) {
    for (int i = 0; i < CLIENT_MAX_REMOTES; i++) {
        if (!g_remotes[i].active) {
            memset(&g_remotes[i], 0, sizeof(g_remotes[i]));
            g_remotes[i].active = 1;
            g_remotes[i].client_id = client_id;
            return &g_remotes[i];
        }
    }
    return NULL;
}

int network_connect_and_handshake(const char* host, int port, uint64_t* out_seed, const char* nickname) {
    if (!host || !out_seed) return -1;

    g_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (g_sock < 0) {
        perror("socket");
        return -1;
    }

    set_nonblocking(g_sock);

    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons(port);

    if (inet_pton(AF_INET, host, &a.sin_addr) <= 0) {
        perror("inet_pton");
        close(g_sock);
        g_sock = -1;
        return -1;
    }

    int ret = connect(g_sock, (struct sockaddr*)&a, sizeof(a));
    if (ret < 0 && errno != EINPROGRESS) {
        perror("connect");
        close(g_sock);
        g_sock = -1;
        return -1;
    }

    fd_set writefds;
    FD_ZERO(&writefds);
    FD_SET(g_sock, &writefds);
    
    struct timeval tv;
    tv.tv_sec = 5; // 5 sec
    tv.tv_usec = 0;
    
    ret = select(g_sock + 1, NULL, &writefds, NULL, &tv);
    if (ret <= 0) {
        fprintf(stderr, "Connection timeout\n");
        close(g_sock);
        g_sock = -1;
        return -1;
    }

    int so_error;
    socklen_t len = sizeof(so_error);
    getsockopt(g_sock, SOL_SOCKET, SO_ERROR, &so_error, &len);
    if (so_error != 0) {
        fprintf(stderr, "Connection failed: %s\n", strerror(so_error));
        close(g_sock);
        g_sock = -1;
        return -1;
    }

    // now sending (socket ready)
    HandshakePacket handshake_send;
    memset(&handshake_send, 0, sizeof(handshake_send));
    handshake_send.type = PKT_HANDSHAKE;
    handshake_send.client_id = 0;
    handshake_send.seed = 0;
    strncpy(handshake_send.nickname, nickname, MAX_NICKNAME_LEN - 1);
    handshake_send.nickname[MAX_NICKNAME_LEN - 1] = '\0';
    
    ssize_t sent = send(g_sock, &handshake_send, sizeof(handshake_send), 0);
    if (sent != sizeof(handshake_send)) {
        if (sent < 0) perror("send handshake");
        else fprintf(stderr, "send handshake: incomplete\n");
        close(g_sock);
        g_sock = -1;
        return -1;
    }

    printf("Handshake sent, waiting for response...\n");

    for (int i = 0; i < 10000; i++) {
        HandshakePacket hp;
        memset(&hp, 0, sizeof(hp));

        size_t got = 0;
        uint8_t* dst = (uint8_t*)&hp;

        while (got < sizeof(HandshakePacket)) {
            ssize_t n = recv(g_sock, dst + got, sizeof(HandshakePacket) - got, 0);
            if (n > 0) {
                got += (size_t)n;
                continue;
            }
            if (n == 0) {
                break;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(5000);  // 5ms sleep
                break;
            }
            perror("recv");
            break;
        }

        if (got == sizeof(HandshakePacket) && hp.type == PKT_HANDSHAKE) {
            g_seed = hp.seed;
            *out_seed = hp.seed;
            g_local_client_id = hp.client_id;
            printf("Connected. client_id: %u, seed: %llu\n", g_local_client_id, (unsigned long long)hp.seed);
            recv_buffer_len = 0;
            return 0;
        }
        usleep(5000);
    }

    fprintf(stderr, "Handshake timed out\n");
    close(g_sock);
    g_sock = -1;
    return -1;
}

void network_send_player_state(uint32_t client_id, const float pos[3], const float rot[3], uint8_t on_ground) {
    if (g_sock < 0) return;

    extern char __nickname[32];
    PlayerStatePacket p;
    p.type = PKT_PLAYER_STATE;
    p.client_id = client_id;
    memcpy(p.pos, pos, sizeof(p.pos));
    memcpy(p.rot, rot, sizeof(p.rot));
    p.on_ground = on_ground;
    strncpy(p.nickname, __nickname, MAX_NICKNAME_LEN - 1);
    p.nickname[MAX_NICKNAME_LEN - 1] = '\0';

    send(g_sock, &p, sizeof(p), 0);
}

void network_send_block_change(uint32_t client_id, int32_t x, int32_t y, int32_t z, uint16_t type) {
    if (g_sock < 0) return;

    BlockUpdatePacket b;
    b.type = PKT_BLOCK_UPDATE;
    b.block_type = type;
    b.x = x;
    b.y = y;
    b.z = z;

    send(g_sock, &b, sizeof(b), 0);
}

static void process_packet(const uint8_t* data, size_t len) {
    if (len < 1) return;
    
    uint8_t type = data[0];

    if (type == PKT_PLAYER_SPAWN) {
        if (len < sizeof(PlayerSpawnPacket)) return;
        PlayerSpawnPacket* sp = (PlayerSpawnPacket*)data;

        RemotePlayer* rp = find_remote(sp->client_id);
        if (!rp) rp = alloc_remote(sp->client_id);
        if (!rp) return;

        memcpy(rp->pos, sp->pos, sizeof(rp->pos));
        memcpy(rp->rot, sp->rot, sizeof(rp->rot));
        rp->on_ground = 0;
        strncpy(rp->nickname, sp->nickname, MAX_NICKNAME_LEN - 1);
        rp->nickname[MAX_NICKNAME_LEN - 1] = '\0';
        printf("Remote connected: id=%u, nickname='%s'\n", sp->client_id, rp->nickname);
        return;
    }

    if (type == PKT_PLAYER_DESPAWN) {
        if (len < sizeof(PlayerDespawnPacket)) return;
        PlayerDespawnPacket* dp = (PlayerDespawnPacket*)data;

        RemotePlayer* rp = find_remote(dp->client_id);
        if (rp) {
            printf("Remote disconnected: id=%u, nickname='%s'\n", dp->client_id, rp->nickname);
            rp->active = 0;
        }
        return;
    }

    if (type == PKT_PLAYER_STATE) {
        if (len < sizeof(PlayerStatePacket)) return;
        PlayerStatePacket* st = (PlayerStatePacket*)data;

        if (st->client_id == g_local_client_id) return;

        RemotePlayer* rp = find_remote(st->client_id);
        if (!rp) {
            rp = alloc_remote(st->client_id);
            if (!rp) return;
            printf("Remote state (implicit spawn): id=%u, nickname='%s'\n", st->client_id, st->nickname);
        }

        memcpy(rp->pos, st->pos, sizeof(rp->pos));
        memcpy(rp->rot, st->rot, sizeof(rp->rot));
        rp->on_ground = st->on_ground;
        if (strlen(st->nickname) > 0) {
            strncpy(rp->nickname, st->nickname, MAX_NICKNAME_LEN - 1);
            rp->nickname[MAX_NICKNAME_LEN - 1] = '\0';
        }
        return;
    }

    if (type == PKT_BLOCK_UPDATE) {
        if (len < sizeof(BlockUpdatePacket)) return;
        BlockUpdatePacket* bu = (BlockUpdatePacket*)data;
        extern World world;
        world_set_block(&world, bu->x, bu->y, bu->z, bu->block_type);
        rebuild_chunks_for_block(&world, bu->x, bu->y, bu->z);
        return;
    }

    if (type == PKT_WORLD_SNAPSHOT) {
        if (len < sizeof(WorldSnapshotPacket)) return;
        WorldSnapshotPacket* snap = (WorldSnapshotPacket*)data;
        BlockChangeData* blocks = (BlockChangeData*)(data + sizeof(WorldSnapshotPacket));
        
        printf("[client] Received world snapshot with %u block changes\n", snap->count);
        
        extern World world;
        for (uint32_t i = 0; i < snap->count; i++) {
            world_set_block(&world, blocks[i].x, blocks[i].y, blocks[i].z, blocks[i].block_type);
            rebuild_chunks_for_block(&world, blocks[i].x, blocks[i].y, blocks[i].z);
        }
        return;
    }
}

void network_pump(void) {
    if (g_sock < 0) return;

    uint8_t temp[4096];
    ssize_t n = recv(g_sock, temp, sizeof(temp), MSG_DONTWAIT);
    
    if (n > 0) {
        if (recv_buffer_len + n > sizeof(recv_buffer)) {
            // overflow
            fprintf(stderr, "Network: receive buffer overflow, resetting\n");
            recv_buffer_len = 0;
            return;
        }
        memcpy(recv_buffer + recv_buffer_len, temp, n);
        recv_buffer_len += n;
        
        // processing multiple packets
        size_t offset = 0;
        while (offset < recv_buffer_len) {
            if (recv_buffer_len - offset < 1) break;
            
            uint8_t type = recv_buffer[offset];
            size_t pkt_size = 0;
            
            switch (type) {
                case PKT_PLAYER_SPAWN:
                    pkt_size = sizeof(PlayerSpawnPacket);
                    break;
                case PKT_PLAYER_DESPAWN:
                    pkt_size = sizeof(PlayerDespawnPacket);
                    break;
                case PKT_PLAYER_STATE:
                    pkt_size = sizeof(PlayerStatePacket);
                    break;
                case PKT_BLOCK_UPDATE:
                    pkt_size = sizeof(BlockUpdatePacket);
                    break;
                case PKT_WORLD_SNAPSHOT:
                    if (recv_buffer_len - offset >= sizeof(WorldSnapshotPacket)) {
                        WorldSnapshotPacket* snap = (WorldSnapshotPacket*)(recv_buffer + offset);
                        pkt_size = sizeof(WorldSnapshotPacket) + snap->count * sizeof(BlockChangeData);
                    } else {
                        pkt_size = 0;
                    }
                    break;
                default:
                    // Unknown packet type - skip 1 byte to avoid infinite loop
                    fprintf(stderr, "Network: unknown packet type %d, skipping\n", type);
                    pkt_size = 1;
                    break;
            }
            
            if (pkt_size > 0 && recv_buffer_len - offset >= pkt_size) {
                process_packet(recv_buffer + offset, pkt_size);
                offset += pkt_size;
            } else {
                break;
            }
        }
        
        if (offset > 0) {
            memmove(recv_buffer, recv_buffer + offset, recv_buffer_len - offset);
            recv_buffer_len -= offset;
        }
    }
    else if (n == 0) {
        close(g_sock);
        g_sock = -1;
        recv_buffer_len = 0;
        return;
    }
    else if (n < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            close(g_sock);
            g_sock = -1;
            recv_buffer_len = 0;
        }
    }
}