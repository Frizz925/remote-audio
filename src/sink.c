#include <stdio.h>
#include <stdlib.h>

#include "crypto.h"
#include "proto.h"
#include "stream.h"
#include "types.h"
#include "utils.h"

#define MAX_STREAMS 16

typedef struct {
    ra_keypair_t *keypair;
} ra_sink_t;

typedef struct {
    ra_sink_t *sink;
    const ra_conn_t *conn;
    const char *buf;
    size_t len;
} ra_handler_context_t;

static ra_stream_t streams[MAX_STREAMS] = {0};

static void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    buf->base = malloc(suggested_size);
    buf->len = suggested_size;
}

static void create_handshake_response_message(ra_stream_t *stream, const unsigned char *pubkey, size_t keylen) {
    char *wptr = stream->rawbuf;
    *wptr++ = (char)RA_HANDSHAKE_RESPONSE;
    *wptr++ = (char)stream->id;
    *wptr++ = (char)keylen;
    memcpy(wptr, pubkey, keylen);
    stream->buf.len = wptr - stream->rawbuf + keylen;
}

static void handle_stream_data(ra_handler_context_t *ctx, ra_stream_t *stream) {
    printf("Stream %d: Received %zu byte(s)\n", stream->id, ctx->len);
}

static void handle_stream_terminate(ra_handler_context_t *ctx, ra_stream_t *stream) {
    stream->state = 0;
    printf("Stream %d: Terminated\n", stream->id);
}

static void handle_handshake_init(ra_handler_context_t *ctx) {
    if (ctx->len < 1) return;

    ra_sink_t *sink = ctx->sink;
    const ra_keypair_t *keypair = sink->keypair;
    const ra_conn_t *conn = ctx->conn;

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

    const char *rptr = ctx->buf;
    size_t keysize = *rptr++;
    int err = ra_compute_shared_secret(stream->secret, sizeof(stream->secret), (unsigned char *)rptr, keysize, keypair,
                                       RA_SHARED_SECRET_SERVER);
    if (err) {
        fprintf(stderr, "Key exchange failed\n");
        return;
    }
    ra_stream_init(stream, id);

    char straddr[16];
    struct sockaddr_in *saddr = (struct sockaddr_in *)conn->addr;
    uv_ip4_name(saddr, straddr, sizeof(straddr));
    printf("Opened stream %d for source from %s:%d\n", stream->id, straddr, ntohs(saddr->sin_port));

    create_handshake_response_message(stream, keypair->public, sizeof(keypair->public));
    uv_udp_send(&stream->req, conn->sock, &stream->buf, 1, conn->addr, NULL);
}

static void handle_message_crypto(ra_handler_context_t *ctx) {
    if (ctx->len < 1) return;

    // Get the stream by ID
    const char *p = ctx->buf;
    uint8_t stream_id = *p++;
    if (stream_id >= MAX_STREAMS) return;
    ra_stream_t *stream = &streams[stream_id];
    if (stream->state <= 0) return;

    // Read the payload
    char buf[65535];
    size_t buflen = sizeof(buf);
    if (ra_stream_read(stream, buf, &buflen, p, ctx->len - 1)) return;

    // Handle crypto message
    const char *q = buf;
    ra_crypto_type crypto_type = *q++;
    ra_handler_context_t crypto_ctx = {
        .sink = ctx->sink,
        .conn = ctx->conn,
        .buf = q,
        .len = buflen - 1,
    };
    switch (crypto_type) {
    case RA_STREAM_DATA:
        handle_stream_data(&crypto_ctx, stream);
        break;
    case RA_STREAM_TERMINATE:
        handle_stream_terminate(&crypto_ctx, stream);
        break;
    default:
        break;
    }
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
    ra_conn_t conn = {sock, addr};

    const char *rptr = buf->base;
    ra_message_type msg_type = (ra_message_type)*rptr++;
    ra_handler_context_t ctx = {
        .sink = sock->data,
        .conn = &conn,
        .buf = rptr,
        .len = nread - 1,
    };
    switch (msg_type) {
    case RA_HANDSHAKE_INIT:
        handle_handshake_init(&ctx);
        break;
    case RA_MESSAGE_CRYPTO:
        handle_message_crypto(&ctx);
        break;
    default:
        break;
    }
}

int main() {
    int err = 0;
    ra_sink_t sink;

    if (ra_crypto_init()) {
        goto error;
    }
    ra_keypair_t keypair;
    ra_generate_keypair(&keypair);
    sink.keypair = &keypair;

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

    register_signals(loop, NULL, NULL);
    int rc = uv_run(loop, UV_RUN_DEFAULT);
    printf("Sink stopped listening.\n");
    return rc;

error:
    return EXIT_FAILURE;
}
