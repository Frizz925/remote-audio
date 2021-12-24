#ifndef _RA_STREAM_H
#define _RA_STREAM_H

#include <arpa/inet.h>
#include <stdint.h>
#include <uv.h>

#include "crypto.h"

typedef struct {
    uint8_t id;
    uint8_t state;
    uint64_t prev_nonce;
    uint8_t secret[SHARED_SECRET_SIZE];
    uv_udp_send_t req;
    uv_buf_t buf;
    char rawbuf[256];
} ra_stream_t;

typedef struct {
    uv_udp_t *sock;
    const struct sockaddr *addr;
} ra_conn_t;

void ra_stream_init(ra_stream_t *stream, uint8_t id);
int ra_stream_send(ra_stream_t *stream, const ra_conn_t *conn, const char *buf, size_t len, uv_udp_send_cb send_cb);
int ra_stream_read(ra_stream_t *stream, char *outbuf, size_t *outlen, const char *buf, size_t len);

#endif
