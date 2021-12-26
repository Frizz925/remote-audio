#ifndef _RA_STREAM_H
#define _RA_STREAM_H

#include <stdint.h>

#include "crypto.h"
#include "socket.h"
#include "types.h"

#define BUFSIZE 65535

typedef struct {
    SOCKET sock;
    struct sockaddr *addr;
    socklen_t addrlen;
} ra_conn_t;

typedef struct {
    char *base;
    size_t len;
    size_t cap;
} ra_buf_t;

typedef struct {
    const char *base;
    size_t len;
} ra_rbuf_t;

typedef struct {
    uint8_t id;
    uint64_t prev_nonce;
    uint8_t secret[SHARED_SECRET_SIZE];
} ra_stream_t;

void ra_buf_init(ra_buf_t *buf, char *rawbuf, size_t size);
void ra_rbuf_init(ra_rbuf_t *buf, const char *rawbuf, size_t len);

ssize_t ra_buf_recv(ra_conn_t *conn, ra_buf_t *buf);
ssize_t ra_buf_send(const ra_conn_t *conn, const ra_rbuf_t *buf);

ra_stream_t *ra_stream_create(uint8_t id);
void ra_stream_init(ra_stream_t *stream, uint8_t id);
void ra_stream_reset(ra_stream_t *stream);
int ra_stream_read(ra_stream_t *stream, ra_buf_t *buf, const char *inbuf, size_t len);
int ra_stream_write(ra_stream_t *stream, char *outbuf, size_t *outlen, const ra_rbuf_t *buf);
ssize_t ra_stream_send(ra_stream_t *stream, const ra_conn_t *conn, const ra_rbuf_t *buf);
void ra_stream_destroy(ra_stream_t *stream);

#endif
