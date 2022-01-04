#ifndef _RA_STREAM_H
#define _RA_STREAM_H

#include <stdatomic.h>
#include <stdint.h>

#include "proto.h"

#define BUFSIZE 65535

typedef struct {
    uint8_t id;
    uint8_t secret[SHARED_SECRET_SIZE];
    atomic_ullong read_nonce;
    atomic_ullong write_nonce;
} ra_stream_t;

ra_stream_t *ra_stream_create(uint8_t id);
void ra_stream_init(ra_stream_t *stream, uint8_t id);
void ra_stream_reset(ra_stream_t *stream);
int ra_stream_read(ra_stream_t *stream, ra_buf_t *buf, const char *inbuf, size_t len);
int ra_stream_write(ra_stream_t *stream, char *outbuf, size_t *outlen, const ra_rbuf_t *buf);
ssize_t ra_stream_send(ra_stream_t *stream, const ra_conn_t *conn, const ra_rbuf_t *buf);
void ra_stream_destroy(ra_stream_t *stream);

#endif
