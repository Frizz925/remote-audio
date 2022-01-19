#include "socket.h"

#include <uv.h>

const char *straddr(const struct sockaddr *addr) {
    static char buf[32];
    const struct sockaddr_in *inaddr;
    const struct sockaddr_in6 *in6addr;
    char *wptr = buf;
    char *endptr = wptr + sizeof(buf);

    switch (addr->sa_family) {
    case AF_INET:
        inaddr = (struct sockaddr_in *)addr;
        uv_ip4_name(inaddr, wptr, endptr - wptr);
        wptr += strlen(buf);
        wptr += snprintf(wptr, endptr - wptr, ":%d", ntohs(inaddr->sin_port));
        break;
    case AF_INET6:
        in6addr = (struct sockaddr_in6 *)addr;
        uv_ip6_name(in6addr, wptr, endptr - wptr);
        wptr += strlen(buf);
        wptr += snprintf(wptr, endptr - wptr, ":%d", ntohs(in6addr->sin6_port));
        break;
    default:
        return NULL;
    }

    return buf;
}
