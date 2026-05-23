#include "network.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "packets.h"

// a little abstraction layer between raw ass sockets and our prestige ccraft client

static int g_sock = -1;
static uint64_t g_seed = 0;
static uint32_t g_local_client_id = 0;
static RemotePlayer g_remotes[CLIENT_MAX_REMOTES];

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

int network_connect_and_handshake(const char* host, int port, uint64_t* out_seed) {
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
    if (ret < 0) {
        if (errno != EINPROGRESS) {
            perror("connect");
            close(g_sock);
            g_sock = -1;
            return -1;
        }
    }

    for (int i = 0; i < 2000; i++) {
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
                usleep(1000);
                continue;
            }
            break;
        }

        if (got == sizeof(HandshakePacket) && hp.type == PKT_HANDSHAKE) {
            g_seed = hp.seed;
            *out_seed = hp.seed;
            g_local_client_id = hp.client_id;
            printf("Connected. client_id: %u, seed: %llu\n", g_local_client_id, (unsigned long long)hp.seed);
            return 0;
        }
        usleep(1000);
    }

    fprintf(stderr, "Handshake timed out\n");
    close(g_sock);
    g_sock = -1;
    return -1;
}

void network_send_player_state(uint32_t client_id, const float pos[3], const float rot[3], uint8_t on_ground) {
    if (g_sock < 0) return;

    PlayerStatePacket p;
    p.type = PKT_PLAYER_STATE;
    p.client_id = client_id;
    memcpy(p.pos, pos, sizeof(p.pos));
    memcpy(p.rot, rot, sizeof(p.rot));
    p.on_ground = on_ground;

    send(g_sock, &p, sizeof(p), 0);
}

static void handle_packet(uint8_t* buffer, ssize_t bytes) {
    if (bytes <= 0) return;

    uint8_t type = buffer[0];

    if (type == PKT_PLAYER_SPAWN) {
        if (bytes < (ssize_t)sizeof(PlayerSpawnPacket)) return;
        PlayerSpawnPacket* sp = (PlayerSpawnPacket*)buffer;

        RemotePlayer* rp = find_remote(sp->client_id);
        if (!rp) rp = alloc_remote(sp->client_id);
        if (!rp) return;

        memcpy(rp->pos, sp->pos, sizeof(rp->pos));
        memcpy(rp->rot, sp->rot, sizeof(rp->rot));
        rp->on_ground = 0;
        printf("Remote connected: id=%u\n", sp->client_id);
        return;
    }

    if (type == PKT_PLAYER_DESPAWN) {
        if (bytes < (ssize_t)sizeof(PlayerDespawnPacket)) return;
        PlayerDespawnPacket* dp = (PlayerDespawnPacket*)buffer;

        RemotePlayer* rp = find_remote(dp->client_id);
        if (rp) {
            rp->active = 0;
            printf("Remote disconnected: id=%u\n", dp->client_id);
        }
        return;
    }

    if (type == PKT_PLAYER_STATE) {
        if (bytes < (ssize_t)sizeof(PlayerStatePacket)) return;
        PlayerStatePacket* st = (PlayerStatePacket*)buffer;

        if (st->client_id == g_local_client_id) return;

        RemotePlayer* rp = find_remote(st->client_id);
        if (!rp) {
            rp = alloc_remote(st->client_id);
            if (!rp) return;
            printf("Remote state (implicit spawn): id=%u\n", st->client_id);
        }

        memcpy(rp->pos, st->pos, sizeof(rp->pos));
        memcpy(rp->rot, st->rot, sizeof(rp->rot));
        rp->on_ground = st->on_ground;
        return;
    }

    if (type == PKT_BLOCK_UPDATE || type == PKT_CHUNK_DATA) {
        return;
    }
}

void network_pump(void) {
    if (g_sock < 0) return;

    uint8_t buffer[4096];

    while (1) {
        ssize_t n = recv(g_sock, buffer, sizeof(buffer), MSG_DONTWAIT);
        if (n > 0) {
            handle_packet(buffer, n);
            continue;
        }
        if (n == 0) {
            close(g_sock);
            g_sock = -1;
            return;
        }
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return;
            return;
        }
    }
}