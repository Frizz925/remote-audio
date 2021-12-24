#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "crypto.h"
#include "proto.h"
#include "stream.h"
#include "types.h"

#define MAX_STREAMS 16

typedef struct {
    ra_keypair_t keypair;
} ra_sink_t;

typedef struct {
    ra_sink_t *sink;
    const ra_conn_t *conn;
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
    ra_sink_t *sink = ctx->sink;
    const ra_keypair_t *keypair = &sink->keypair;
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

    const char *p = ctx->buf;
    size_t keysize = *p++;
    int err = ra_compute_shared_secret(stream->secret, sizeof(stream->secret), (unsigned char *)p, keysize, keypair,
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

    char rawbuf[256];
    stream->buf.len =
        create_init_response_message(stream->rawbuf, stream->id, keypair->public, sizeof(keypair->public));
    uv_udp_send(&stream->req, conn->sock, &stream->buf, 1, conn->addr, NULL);
}

static void handle_stream_data(ra_stream_context_t *ctx) {
    const char *p = ctx->buf;
    uint8_t stream_id = *p++;
    if (stream_id >= MAX_STREAMS) return;
    ra_stream_t *stream = &streams[stream_id];
    if (stream->state <= 0) return;

    char buf[256];
    size_t buflen = sizeof(buf);
    int err = ra_stream_read(stream, buf, &buflen, p, ctx->len - 1);
    if (err) return;
    printf("Stream %d: Received %zu byte(s)\n", stream->id, buflen);
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

    char *p = buf->base;
    ra_message_type msg_type = (ra_message_type)*p++;
    ra_stream_context_t ctx = {sock->data, &conn, p, nread - 1};
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
