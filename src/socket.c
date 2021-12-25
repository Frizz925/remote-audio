#include "socket.h"

#if defined(WIN32) || defined(_WIN32)
#else
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int ra_socket_init() {
    return 0;
}

void ra_socket_close(SOCKET sock) {
    close(sock);
}

void ra_socket_deinit() {}

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
#endif
