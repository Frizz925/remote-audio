#include "inet.h"

#include <stdio.h>

int straddr(char *result, struct sockaddr_in *addr_in) {
    char buf[INET_ADDRSTRLEN];
    const char *addr = inet_ntop(AF_INET, &addr_in->sin_addr, buf, INET_ADDRSTRLEN);
    return sprintf(result, "%s:%d", addr, ntohs(addr_in->sin_port));
}