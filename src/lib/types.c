#include "types.h"

#include "string.h"

void uint16_to_bytes(char *buf, uint16_t value) {
    uint16_raw_t raw = {.value = htons(value)};
    memcpy(buf, raw.buf, sizeof(raw));
}

void uint32_to_bytes(char *buf, uint32_t value) {
    uint32_raw_t raw = {.value = htonl(value)};
    memcpy(buf, raw.buf, sizeof(raw));
}

void uint64_to_bytes(char *buf, uint64_t value) {
    uint64_raw_t raw = {.value = htonll(value)};
    memcpy(buf, raw.buf, sizeof(raw));
}

uint16_t bytes_to_uint16(const char *buf) {
    uint16_raw_t raw;
    memcpy(raw.buf, buf, sizeof(raw));
    return ntohs(raw.value);
}

uint32_t bytes_to_uint32(const char *buf) {
    uint32_raw_t raw;
    memcpy(raw.buf, buf, sizeof(raw));
    return ntohl(raw.value);
}

uint64_t bytes_to_uint64(const char *buf) {
    uint64_raw_t raw;
    memcpy(raw.buf, buf, sizeof(raw));
    return ntohll(raw.value);
}
