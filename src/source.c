#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "crypto.h"
#include "proto.h"
#include "types.h"

#define SEND_BUFSIZE 512

typedef struct {
    uint8_t id;
    uint8_t secret[SHARED_SECRET_SIZE];
    uint8_t state;
    uv_udp_send_t req;
    uv_buf_t buf;
    char rawbuf[256];
} ra_stream_t;

typedef struct {
    ra_keypair_t keypair;
    ra_stream_t stream;
} ra_source_t;

typedef struct {
    uv_udp_t *sock;
    const struct sockaddr *addr;
    const char *buf;
    size_t len;
} ra_stream_context_t;

static void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    buf->base = malloc(suggested_size);
    buf->len = suggested_size;
}

static size_t create_init_message(char *buf, const unsigned char *pubkey, size_t keylen) {
    char *p = buf;
    *p++ = (char)RA_STREAM_INIT;
    *p++ = (char)keylen;
    memcpy(p++, pubkey, keylen);
    return 2 + keylen;
}

static size_t create_data_message(char *outbuf, size_t outlen, ra_stream_t *stream, const char *buf, size_t len) {
    static atomic_ulong nonce_counter = 0;

    char nonce[NONCE_SIZE] = {0};
    uint32_to_bytes(nonce, ++nonce_counter);

    char *p = outbuf;
    *p++ = (char)RA_STREAM_DATA;
    *p++ = (char)stream->id;
    memcpy(p, nonce, NONCE_SIZE);
    p += 2 + NONCE_SIZE;

    uint64_t encsize = outlen - (p - outbuf);
    crypto_aead_xchacha20poly1305_ietf_encrypt((unsigned char *)p, &encsize, (unsigned char *)buf, len, NULL, 0, NULL,
                                               (unsigned char *)nonce, stream->secret);
    uint16_to_bytes(p - 2, encsize);
    return p - outbuf + encsize;
}

static void handle_stream_init_response(ra_stream_context_t *ctx) {
    const char *p = ctx->buf;
    ra_source_t *source = (ra_source_t *)ctx->sock->data;
    ra_stream_t *stream = &source->stream;
    stream->id = (uint8_t)*p++;

    unsigned char keysize = (unsigned char)*p++;
    int err = ra_compute_shared_secret(stream->secret, sizeof(stream->secret), (unsigned char *)p, keysize,
                                       &source->keypair, RA_SHARED_SECRET_CLIENT);
    if (err) {
        fprintf(stderr, "Handshake failed with the sink: key exchange failed\n");
        return;
    }
    printf("Handshake with the sink succeed. Proceeding to stream audio to sink.\n");
    stream->state = 1;

    stream->buf.len = create_data_message(stream->rawbuf, sizeof(stream->rawbuf), stream, "Hello, world!", 13);
    uv_udp_send(&stream->req, ctx->sock, &stream->buf, 1, ctx->addr, NULL);
}

static void on_read(uv_udp_t *sock, ssize_t nread, const uv_buf_t *buf, const struct sockaddr *addr, unsigned flags) {
    if (nread < 0) {
        print_uv_error("uv_udp_recv_cb", nread);
        uv_udp_recv_stop(sock);
        free(buf->base);
        return;
    }
    char *p = buf->base;
    ra_message_type msg_type = (ra_message_type)*p++;
    ra_stream_context_t ctx = {sock, addr, p, nread - 1};
    switch (msg_type) {
    case RA_STREAM_INIT_RESPONSE:
        handle_stream_init_response(&ctx);
        break;
    default:
        break;
    }
}

int main(int argc, const char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <sink-host> [sink-port]\n", argv[0]);
        return EXIT_FAILURE;
    }
    const char *host = argv[1];
    int port = LISTEN_PORT;
    if (argc >= 3) {
        port = atoi(argv[2]);
    }
    if (ra_crypto_init()) {
        goto error;
    }
    ra_source_t source;

    ra_keypair_t *keypair = &source.keypair;
    ra_generate_keypair(keypair);

    ra_stream_t *stream = &source.stream;
    stream->buf.base = stream->rawbuf;
    stream->buf.len = sizeof(stream->rawbuf);

    uv_loop_t *loop = uv_default_loop();
    uv_udp_t sock = {.data = &source};
    uv_udp_init(loop, &sock);

    struct sockaddr_in client_addr, server_addr;
    uv_ip4_addr(host, 0, &client_addr);
    uv_ip4_addr(host, port, &server_addr);
    uv_udp_bind(&sock, (struct sockaddr *)&client_addr, SO_REUSEADDR);
    uv_udp_recv_start(&sock, alloc_buffer, on_read);

    printf("Initiating handshake with sink at %s:%d\n", host, port);
    stream->buf.len = create_init_message(stream->rawbuf, keypair->public, keypair->public_size);
    uv_udp_send(&stream->req, &sock, &stream->buf, 1, (struct sockaddr *)&server_addr, NULL);

    register_signals(loop);
    int rc = uv_run(loop, UV_RUN_DEFAULT);
    printf("Source stopped.");
    return rc;

error:
    return EXIT_FAILURE;
}
