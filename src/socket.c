#include "socket.h"

#include <stdio.h>

#ifdef _WIN32
int socket_startup() {
    WSADATA wsaData;
    return WSAStartup(MAKEWORD(2, 2), &wsaData);
}

int socket_cleanup() {
    return WSACleanup();
}

int socket_close(Socket sock) {
    int err = shutdown(sock, SD_BOTH);
    if (err) {
        return err;
    }
    return closesocket(sock);
}
#else
#include <unistd.h>

int socket_startup() {
    return 0;
}

int socket_cleanup() {
    return 0;
}

int socket_close(Socket sock) {
    return close(sock);
}
#endif

Socket socket_accept(Socket sock, struct sockaddr *addr, socklen_t *addrlen) {
    return accept(sock, addr, addrlen);
}

int socket_address(char *buf, const struct sockaddr_in *addr_in) {
    return sprintf(buf, "%s:%d", inet_ntoa(addr_in->sin_addr), ntohs(addr_in->sin_port));
}