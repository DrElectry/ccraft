#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "packets.h"
#include "globals.h"

int main()
{
    int s = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));

    a.sin_family = AF_INET;
    a.sin_port = htons(SERVER_PORT);
    a.sin_addr.s_addr = INADDR_ANY;

    bind(s, (struct sockaddr*)&a, sizeof(a));
    listen(s, 10);

    printf("Server is listening on port %d...\n", SERVER_PORT);

    while (1)
    {
        int c = accept(s, 0, 0);
        Seedpckt p;

        p.type = PKT_SEED;
        p.seed = WORLD_SEED;

        send(c, &p, sizeof(p), 0);
        close(c);
    }
}