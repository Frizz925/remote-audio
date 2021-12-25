#include "socket.h"

#if defined(WIN32) || defined(_WIN32)
#else
#include <unistd.h>

int ra_socket_init() {
    return 0;
}

void ra_socket_close(SOCKET sock) {
    close(sock);
}

void ra_socket_deinit() {}

void ra_sockaddr_init(const char *host, unsigned int port, struct sockaddr_in *saddr) {
    saddr->sin_family = AF_INET;
    saddr->sin_addr.s_addr = inet_addr(host);
    saddr->sin_port = htons(port);
}
#endif
