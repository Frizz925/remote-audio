#include "socket.h"
#include <stdio.h>

#ifdef _WIN32
int ra_socket_init() {
    WORD wVersionRequested;
    WSADATA wsaData;

    wVersionRequested = MAKEWORD(2, 2);
    int err = WSAStartup(wVersionRequested, &wsaData);
    if (err) {
        fprintf(stderr, "WSAStartup failed code %d\n", err);
        return err;
    }
    return 0;
}

void ra_socket_close(SOCKET sock) {
    closesocket(sock);
}

void ra_socket_deinit() {
    WSACleanup();
}
#else
#include <netdb.h>
#include "string.h"
#include <unistd.h>

int ra_socket_init() {
    return 0;
}

void ra_socket_close(SOCKET sock) {
    close(sock);
}

void ra_socket_deinit() {}
#endif

int ra_sockaddr_init(const char *host, unsigned int port, struct sockaddr_in *saddr) {
    struct addrinfo hints = {0}, *addrinfo;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    int err = getaddrinfo(host, NULL, &hints, &addrinfo);
    if (err) {
        perror("getaddrinfo");
        return err;
    }

    memcpy(saddr, addrinfo->ai_addr, sizeof(struct sockaddr_in));
    saddr->sin_port = htons(port);
    freeaddrinfo(addrinfo);

    return 0;
}
