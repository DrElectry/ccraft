#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "packets.h"
#include "globals.h"
#include "server.h"

int main() {
    if (net_init() != 0) {
        fprintf(stderr, "Failed to initialize Winsock\n");
        return 1;
    }
    server_init(&global_server);
    server_start(&global_server);
    server_stop(&global_server);
    return 0;
}