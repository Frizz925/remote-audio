#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "crypto.h"
#include "proto.h"
#include "types.h"

#define MAX_STREAMS 16

typedef struct {
    ra_keypair_t keypair;
} ra_sink_t;

typedef struct {
    uint8_t id;
    uint8_t secret[SHARED_SECRET_SIZE];
    uint8_t state;
    uint32_t prev_nonce;
    uv_udp_send_t req;
} ra_stream_t;

typedef struct {
    uv_udp_t *sock;
    const struct sockaddr *addr;
    const char *buf;
    size_t len;
} ra_stream_context_t;

static ra_stream_t streams[MAX_STREAMS] = {0};

static void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    buf->base = malloc(suggested_size);
    buf->len = suggested_size;
}

static size_t create_init_response_message(char *buf, uint8_t stream_id, const unsigned char *pubkey, size_t keylen) {
    char *p = buf;
    *p++ = (char)RA_STREAM_INIT_RESPONSE;
    *p++ = (char)stream_id;
    *p++ = (char)keylen;
    memcpy(p, pubkey, keylen);
    return p - buf + keylen;
}

static void handle_stream_init(ra_stream_context_t *ctx) {
    ra_sink_t *sink = ctx->sock->data;
    ra_keypair_t *keypair = &sink->keypair;

    uint8_t id;
    ra_stream_t *stream = NULL;
    for (id = 0; id < MAX_STREAMS; id++) {
        stream = &streams[id];
        if (stream->state == 0) break;
        stream = NULL;
    }
    if (stream == NULL) {
        fprintf(stderr, "Can't accept any more audio stream\n");
        return;
    }
    stream->id = id;

    const char *p = ctx->buf;
    size_t keysize = *p++;
    int err = ra_compute_shared_secret(stream->secret, sizeof(stream->secret), (unsigned char *)p, keysize, keypair,
                                       RA_SHARED_SECRET_SERVER);
    if (err) {
        fprintf(stderr, "Key exchange failed\n");
        return;
    }
    stream->state = 1;

    char rawbuf[256];
    uv_buf_t buf = {rawbuf, create_init_response_message(rawbuf, stream->id, keypair->public, keypair->public_size)};
    uv_udp_send(&stream->req, ctx->sock, &buf, 1, ctx->addr, NULL);
}

static void handle_stream_data(ra_stream_context_t *ctx) {
    const char *p = ctx->buf;
    uint8_t stream_id = *p++;
    if (stream_id >= MAX_STREAMS) return;
    ra_stream_t *stream = &streams[stream_id];
    if (stream->state <= 0) return;

    const char *nonce_ptr = p;
    uint32_t nonce = bytes_to_uint32(nonce_ptr);
    if (nonce <= stream->prev_nonce) return;
    stream->prev_nonce = nonce;

    size_t msgsize = bytes_to_uint16(p + NONCE_SIZE);
    p += 2 + NONCE_SIZE;

    char buf[256];
    size_t buflen = sizeof(buf);
    int err = crypto_aead_xchacha20poly1305_ietf_decrypt((unsigned char *)buf, (unsigned long long *)&buflen, NULL,
                                                         (unsigned char *)p, msgsize, NULL, 0,
                                                         (unsigned char *)nonce_ptr, stream->secret);
    if (err) return;
    printf("Stream %d: %s\n", stream->id, buf);
}

static void on_read(uv_udp_t *sock, ssize_t nread, const uv_buf_t *buf, const struct sockaddr *addr, unsigned flags) {
    if (nread <= 0) {
        if (nread < 0) {
            print_uv_error("uv_udp_recv_cb", nread);
            uv_udp_recv_stop(sock);
            free(buf->base);
        }
        return;
    }
    char *p = buf->base;
    ra_message_type msg_type = (ra_message_type)*p++;
    ra_stream_context_t ctx = {sock, addr, p, nread - 1};
    switch (msg_type) {
    case RA_STREAM_INIT:
        handle_stream_init(&ctx);
        break;
    case RA_STREAM_DATA:
        handle_stream_data(&ctx);
        break;
    default:
        break;
    }
}

int main() {
    int err = 0;
    if (ra_crypto_init()) {
        goto error;
    }
    ra_sink_t sink;
    ra_generate_keypair(&sink.keypair);

    uv_loop_t *loop = uv_default_loop();
    uv_udp_t sock = {.data = &sink};
    uv_udp_init(loop, &sock);

    struct sockaddr_in listen_addr;
    uv_ip4_addr("0.0.0.0", LISTEN_PORT, &listen_addr);
    if ((err = uv_udp_bind(&sock, (struct sockaddr *)&listen_addr, UV_UDP_REUSEADDR))) {
        print_uv_error("uv_udp_bind", err);
        goto error;
    }
    if ((err = uv_udp_recv_start(&sock, alloc_buffer, on_read))) {
        print_uv_error("uv_udp_recv_start", err);
        goto error;
    }
    printf("Sink listening at port %d.\n", LISTEN_PORT);

    register_signals(loop);
    int rc = uv_run(loop, UV_RUN_DEFAULT);
    printf("Sink stopped listening.\n");
    return rc;

error:
    return EXIT_FAILURE;
}
