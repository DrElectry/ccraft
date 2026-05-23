#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "packets.h"
#include "globals.h"
#include "server.h"

int main() {
    server_init(&global_server);
    server_start(&global_server);
    server_stop(&global_server);
    return 0;
}