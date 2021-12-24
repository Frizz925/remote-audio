#include <arpa/inet.h>

void uint16_to_bytes(char *buf, uint16_t value) {
    value = htons(value);
    buf[0] = (char)value & 0xFF;
    buf[1] = (char)(value >> 8) & 0xFF;
}

void uint32_to_bytes(char *buf, uint32_t value) {
    value = htonl(value);
    buf[0] = (char)value & 0xFF;
    buf[1] = (char)(value >> 8) & 0xFF;
    buf[2] = (char)(value >> 16) & 0xFF;
    buf[3] = (char)(value >> 24) & 0xFF;
}

uint16_t bytes_to_uint16(const char *buf) {
    uint16_t value = buf[0] | (buf[1] << 8);
    return ntohs(value);
}

uint32_t bytes_to_uint32(const char *buf) {
    uint32_t value = buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
    return ntohl(value);
}
