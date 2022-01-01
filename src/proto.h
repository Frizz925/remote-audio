#ifndef _RA_PROTO_H
#define _RA_PROTO_H

#include <stdint.h>

#include "audio.h"
#include "crypto.h"
#include "socket.h"

#define LISTEN_PORT 21500

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
    SOCKET sock;
    struct sockaddr *addr;
    socklen_t addrlen;
} ra_conn_t;

typedef enum {
    RA_UNKNOWN_MESSAGE_TYPE,
    RA_HANDSHAKE_INIT,
    RA_HANDSHAKE_RESPONSE,
    RA_MESSAGE_CRYPTO,
} ra_message_type;

typedef enum {
    RA_UNKNOWN_CRYPTO_TYPE,
    RA_STREAM_DATA,
    RA_STREAM_HEARTBEAT,
    RA_STREAM_TERMINATE,
} ra_crypto_type;

void ra_buf_init(ra_buf_t *buf, char *rawbuf, size_t size);
void ra_rbuf_init(ra_rbuf_t *buf, const char *rawbuf, size_t len);

ssize_t ra_buf_recvfrom(ra_conn_t *conn, ra_buf_t *buf);
ssize_t ra_buf_sendto(const ra_conn_t *conn, const ra_rbuf_t *buf);

void create_handshake_message(ra_buf_t *buf, const ra_keypair_t *keypair, const ra_audio_config_t *cfg);
void create_handshake_response_message(ra_buf_t *buf, uint8_t stream_id, const ra_keypair_t *keypair);
void create_stream_data_message(ra_buf_t *buf, const ra_rbuf_t *rbuf);
void create_stream_heartbeat_message(ra_buf_t *buf);
void create_stream_terminate_message(ra_buf_t *buf);

#endif
