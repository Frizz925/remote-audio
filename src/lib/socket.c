#include "socket.h"

#ifndef _WIN32
#include <netdb.h>
#include <string.h>
#endif

int ra_sockaddr_init(const char *host, unsigned int port, struct sockaddr_in *saddr) {
    struct addrinfo hints = {0}, *addrinfo;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    int err = getaddrinfo(host, NULL, &hints, &addrinfo);
    if (err) {
        ra_gai_perror("getaddrinfo", err);
        return err;
    }

    memcpy(saddr, addrinfo->ai_addr, sizeof(struct sockaddr));
    saddr->sin_port = htons(port);
    freeaddrinfo(addrinfo);

    return 0;
}

void ra_sockaddr_str(char *buf, struct sockaddr_in *saddr) {
    char host[16];
    inet_ntop(saddr->sin_family, &saddr->sin_addr, host, sizeof(host));
    sprintf(buf, "%s:%d", host, ntohs(saddr->sin_port));
}
