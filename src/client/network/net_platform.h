#ifndef NET_PLATFORM_H
#define NET_PLATFORM_H

#ifdef _WIN32
    #define _WIN32_WINNT 0x0600
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #pragma comment(lib, "ws2_32.lib")

    typedef SOCKET net_socket_t;
    #define NET_INVALID_SOCKET INVALID_SOCKET
    #define NET_SOCKET_ERROR   SOCKET_ERROR
#else
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
    #include <sys/select.h>

    typedef int net_socket_t;
    #define NET_INVALID_SOCKET (-1)
    #define NET_SOCKET_ERROR   (-1)
#endif

#include <string.h>
#include <stdio.h>

static inline int net_init(void) {
#ifdef _WIN32
    WSADATA wsaData;
    return WSAStartup(MAKEWORD(2, 2), &wsaData);
#else
    return 0;
#endif
}

static inline void net_cleanup(void) {
#ifdef _WIN32
    WSACleanup();
#endif
}

static inline int net_close(net_socket_t sock) {
#ifdef _WIN32
    return closesocket(sock);
#else
    return close(sock);
#endif
}

static inline int net_set_nonblocking(net_socket_t sock) {
#ifdef _WIN32
    u_long mode = 1;
    return ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
}

static inline int net_usleep(unsigned int usec) {
#ifdef _WIN32
    Sleep(usec / 1000);
#else
    usleep(usec);
#endif
    return 0;
}

static inline int net_get_error(void) {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

#ifdef _WIN32
    #define NET_EWOULDBLOCK WSAEWOULDBLOCK
    #define NET_EINPROGRESS WSAEINPROGRESS
    #define NET_EAGAIN      WSAEWOULDBLOCK
#else
    #define NET_EWOULDBLOCK EWOULDBLOCK
    #define NET_EINPROGRESS EINPROGRESS
    #define NET_EAGAIN      EAGAIN
#endif

static inline int net_select(int nfds, fd_set *readfds, fd_set *writefds,
                             fd_set *exceptfds, struct timeval *timeout) {
#ifdef _WIN32
    return select(nfds, readfds, writefds, exceptfds, timeout);
#else
    return select(nfds, readfds, writefds, exceptfds, timeout);
#endif
}

#endif