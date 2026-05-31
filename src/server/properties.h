#ifndef PROPERTIES_H
#define PROPERTIES_H

#include <stdint.h>

#define MAX_IP_LEN 64
#define MAX_NAME_LEN 128
#define MAX_DESC_LEN 512
#define PROPERTIES_FILE "server.properties"

typedef struct {
    char ip[MAX_IP_LEN];
    uint16_t port;
    uint64_t seed;
    char name[MAX_NAME_LEN];
    char description[MAX_DESC_LEN];
} ServerConfig;

void server_config_init(ServerConfig* config);
int server_config_load(ServerConfig* config);
int server_config_save(const ServerConfig* config);

#endif