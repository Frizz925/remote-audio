#include "proto.h"

#include <arpa/inet.h>
#include <string.h>

int proto_write_header(char *stream, int type, int flags) {
    *stream = (type & 0xF) << 4 | (flags & 0xF);
    return 1;
}

int proto_read_header(const char *stream, int *type, int *flags) {
    char bit = *stream;
    *type = (bit >> 4) & 0xF;
    *flags = bit & 0xF;
    return 1;
}

int proto_write_handshake_init(char *stream, const void *pubkey, keylen_t keylen) {
    const uint16 length = htons(keylen);
    memcpy(stream, &length, 2);
    memcpy(stream + 2, pubkey, keylen);
    return keylen + 2;
}

int proto_read_handshake_init(const char *stream, void **pubkey, keylen_t *keylen) {
    uint16 length;
    memcpy(&length, stream, 2);
    length = ntohs(length);
    *pubkey = (void *)(stream + 2);
    *keylen = length;
    return length + 2;
}

int proto_write_handshake_resp(char *stream, const void *pubkey, keylen_t keylen) {
    return proto_write_handshake_init(stream, pubkey, keylen);
}

int proto_read_handshake_resp(const char *stream, void **pubkey, keylen_t *keylen) {
    return proto_read_handshake_init(stream, pubkey, keylen);
}
