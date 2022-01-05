#include <stdint.h>
#include <winsock2.h>

#ifdef __LITTLE_ENDIAN__
uint64_t htonll(uint64_t hostval) {
    return ((uint64_t)htonl(hostval & 0xFFFFFFFF) << 32) | htonl(hostval >> 32);
}

uint64_t ntohll(uint64_t netval) {
    return ((uint64_t)ntohl(netval & 0xFFFFFFFF) << 32) | ntohl(netval >> 32);
}
#else
uint64_t htonll(uint64_t hostval) {
    return hostval;
}

uint64_t ntohll(uint64_t netval) {
    return netval;
}
#endif
