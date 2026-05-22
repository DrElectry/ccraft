#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

int main()
{
    int s = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));

    a.sin_family = AF_INET;
    a.sin_port = htons(25565);
    a.sin_addr.s_addr = INADDR_ANY;

    bind(s, (struct sockaddr*)&a, sizeof(a));
    listen(s, 10);

    printf("Server is listening on port 25565...\n");

    while (1)
    {
        int c = accept(s, 0, 0);
        char m[] = "hi\n";
        send(c, m, sizeof(m), 0);
        close(c);
    }
}