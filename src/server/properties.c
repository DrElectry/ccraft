#include "properties.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void server_config_init(ServerConfig* config) {
    strcpy(config->ip, "0.0.0.0");
    config->port = 25565;
    config->seed = 0x123456789ABCDEF0ULL;
    strcpy(config->name, "CCRAFT Server");
    strcpy(config->description, "Welcome to my server!");
}

int server_config_save(const ServerConfig* config) {
    FILE* file = fopen(PROPERTIES_FILE, "w");
    if (!file) {
        perror("Failed to create server.properties");
        return 0;
    }

    fprintf(file, "# This file was auto generated.\n\n");
    
    fprintf(file, "ip=%s\n", config->ip);
    fprintf(file, "port=%u\n", config->port);
    fprintf(file, "seed=%llu\n", (unsigned long long)config->seed);
    fprintf(file, "name=%s\n", config->name);
    fprintf(file, "desc=%s\n", config->description);
    
    fclose(file);
    return 1;
}

static void trim_whitespace(char* str) {
    char* end;
    while (*str == ' ' || *str == '\t') str++;
    end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) {
        *end = '\0';
        end--;
    }
}

int server_config_load(ServerConfig* config) {
    FILE* file = fopen(PROPERTIES_FILE, "r");
    
    if (!file) {
        server_config_init(config);
        server_config_save(config);
        return 1;
    }
    
    server_config_init(config);
    
    char line[1024];
    
    while (fgets(line, sizeof(line), file)) {
        trim_whitespace(line);
        if (strlen(line) == 0) continue;
        
        char* equals = strchr(line, '=');
        if (!equals) continue;
        
        *equals = '\0';
        char* key = line;
        char* value = equals + 1;
        trim_whitespace(key);
        trim_whitespace(value);
        
        if (strcmp(key, "ip") == 0) {
            strncpy(config->ip, value, MAX_IP_LEN - 1);
            config->ip[MAX_IP_LEN - 1] = '\0';
        } else if (strcmp(key, "port") == 0) {
            config->port = (uint16_t)atoi(value);
        } else if (strcmp(key, "seed") == 0) {
            config->seed = strtoull(value, NULL, 10);
        } else if (strcmp(key, "name") == 0) {
            strncpy(config->name, value, MAX_NAME_LEN - 1);
            config->name[MAX_NAME_LEN - 1] = '\0';
        } else if (strcmp(key, "desc") == 0) {
            strncpy(config->description, value, MAX_DESC_LEN - 1);
            config->description[MAX_DESC_LEN - 1] = '\0';
        }
    }
    
    fclose(file);
    return 1;
}