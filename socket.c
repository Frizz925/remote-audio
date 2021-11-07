#include "socket.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>

int addrstr(char *buf, const struct sockaddr_in *addr_in) {
    char temp[INET_ADDRSTRLEN];
    const char *addr = inet_ntop(addr_in->sin_family, &addr_in->sin_addr, temp, INET_ADDRSTRLEN);
    return sprintf(buf, "%s:%d", addr, ntohs(addr_in->sin_port));
}
