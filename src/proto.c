#include "proto.h"

#include "string.h"

ra_rbuf_t *ra_stream_heartbeat_message = NULL;
ra_rbuf_t *ra_stream_terminate_message = NULL;

static ra_rbuf_t *create_stream_signal_message(ra_crypto_type type) {
    char *ptr = (char *)malloc(1 + sizeof(ra_rbuf_t));
    *ptr = (char)type;

    ra_rbuf_t *rbuf = (ra_rbuf_t *)(ptr + 1);
    rbuf->base = ptr;
    rbuf->len = 1;
    return rbuf;
}

static ra_rbuf_t *create_stream_heartbeat_message() {
    return create_stream_signal_message(RA_STREAM_HEARTBEAT);
}

static ra_rbuf_t *create_stream_terminate_message() {
    return create_stream_signal_message(RA_STREAM_TERMINATE);
}

static void destroy_stream_signal_message(ra_rbuf_t **ptr) {
    if (*ptr == NULL) return;
    free(*ptr);
    *ptr = NULL;
}

void ra_proto_init() {
    ra_stream_heartbeat_message = create_stream_heartbeat_message();
    ra_stream_terminate_message = create_stream_terminate_message();
}

void ra_proto_deinit() {
    destroy_stream_signal_message(&ra_stream_heartbeat_message);
    destroy_stream_signal_message(&ra_stream_terminate_message);
}

void ra_buf_init(ra_buf_t *buf, char *rawbuf, size_t size) {
    buf->base = rawbuf;
    buf->len = 0;
    buf->cap = size;
}

void ra_rbuf_init(ra_rbuf_t *buf, const char *rawbuf, size_t len) {
    buf->base = rawbuf;
    buf->len = len;
}

ssize_t ra_buf_recvfrom(ra_conn_t *conn, ra_buf_t *buf) {
    ssize_t res = recvfrom(conn->sock, buf->base, buf->cap, 0, conn->addr, &conn->addrlen);
    if (res < 0)
        ra_socket_perror("recvfrom");
    else
        buf->len = res;
    return res;
}

ssize_t ra_buf_sendto(const ra_conn_t *conn, const ra_rbuf_t *buf) {
    ssize_t res = sendto(conn->sock, buf->base, buf->len, 0, conn->addr, conn->addrlen);
    if (res < 0) ra_socket_perror("sendto");
    return res;
}

void create_handshake_message(ra_buf_t *buf, const ra_keypair_t *keypair, const ra_audio_config_t *cfg) {
    size_t keylen = sizeof(keypair->public);
    char *p = buf->base;
    *p++ = (char)RA_HANDSHAKE_INIT;
    *p++ = (char)keylen;
    memcpy(p, keypair->public, keylen);
    p += keylen;

    // Inject audio config
    *p++ = (char)cfg->channel_count;
    *p++ = (char)cfg->sample_format;
    uint16_to_bytes(p, cfg->frame_size);
    uint32_to_bytes(p + 2, cfg->sample_rate);
    p += 6;

    buf->len = p - buf->base;
}

void create_handshake_response_message(ra_buf_t *buf, uint8_t stream_id, const ra_keypair_t *keypair) {
    size_t keylen = sizeof(keypair->public);
    char *wptr = buf->base;
    *wptr++ = (char)RA_HANDSHAKE_RESPONSE;
    *wptr++ = (char)stream_id;
    *wptr++ = (char)keylen;
    memcpy(wptr, keypair->public, keylen);
    buf->len = wptr - buf->base + keylen;
}

void create_stream_data_message(ra_buf_t *buf, const ra_rbuf_t *rbuf) {
    buf->base[0] = (char)RA_STREAM_DATA;
    memcpy(buf->base + 1, rbuf->base, rbuf->len);
    buf->len = rbuf->len + 1;
}
