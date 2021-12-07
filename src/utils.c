#include "utils.h"

#include <stdio.h>

int straddr_p(struct sockaddr_in *saddr, char *buf) {
    return sprintf(buf, "%s:%d", inet_ntoa(saddr->sin_addr), ntohs(saddr->sin_port));
}

const char *straddr(struct sockaddr_in *saddr) {
    static char buf[128];
    straddr_p(saddr, buf);
    return buf;
}
