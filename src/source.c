#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "audio.h"
#include "crypto.h"
#include "proto.h"
#include "stream.h"
#include "types.h"
#include "utils.h"

typedef struct {
    ra_conn_t *conn;
    ra_keypair_t *keypair;
    ra_stream_t *stream;
    ra_buf_t *wbuf;
    PaStream *pa_stream;
    OpusEncoder *encoder;
} ra_source_t;

typedef struct {
    const ra_conn_t *conn;
    const ra_rbuf_t *buf;
} ra_handler_context_t;

static SOCKET sock = -1;
static atomic_bool is_running = false;
static ra_source_t *source = NULL;

static void create_handshake_message(ra_buf_t *buf, const unsigned char *pubkey, size_t keylen) {
    char *p = buf->base;
    *p++ = (char)RA_HANDSHAKE_INIT;
    *p++ = (char)keylen;
    memcpy(p++, pubkey, keylen);
    buf->len = 2 + keylen;
}

static void create_terminate_message(ra_buf_t *buf) {
    char *p = buf->base;
    *p++ = (char)RA_STREAM_TERMINATE;
    buf->len = 1;
}

static void send_crypto_data(const ra_conn_t *conn, ra_stream_t *stream, const char *src, size_t len) {
    char buf[65535];
    char *wptr = buf;
    *wptr++ = RA_STREAM_DATA;
    memcpy(wptr, src, len);
    ra_stream_send(stream, conn, buf, len + 1);
}

static void handle_handshake_response(ra_handler_context_t *ctx) {
    static const size_t hdrlen = 2;
    const ra_rbuf_t *buf = ctx->buf;
    if (buf->len < hdrlen) return;
    size_t datalen = buf->len - hdrlen;

    ra_stream_t *stream = source->stream;
    const char *rptr = buf->base;
    stream->id = (uint8_t)*rptr++;

    unsigned char keysize = (unsigned char)*rptr++;
    if (keysize > datalen) return;

    int err = ra_compute_shared_secret(stream->secret, sizeof(stream->secret), (unsigned char *)rptr, keysize,
                                       source->keypair, RA_SHARED_SECRET_CLIENT);
    if (err) {
        fprintf(stderr, "Handshake failed with the sink: key exchange failed\n");
        return;
    }
    printf("Handshake with the sink succeed. Proceeding to stream audio to sink.\n");
    Pa_StartStream(source->pa_stream);
}

static void handle_message(ra_handler_context_t *ctx) {
    const ra_rbuf_t *buf = ctx->buf;
    const char *rptr = buf->base;
    ra_message_type msg_type = (ra_message_type)*rptr++;
    ra_rbuf_t next_buf = {
        .base = rptr,
        .len = buf->len - 1,
    };
    ra_handler_context_t next_ctx = {
        .conn = ctx->conn,
        .buf = &next_buf,
    };
    switch (msg_type) {
    case RA_HANDSHAKE_RESPONSE:
        handle_handshake_response(&next_ctx);
        break;
    default:
        break;
    }
}

static int audio_callback(const void *input, void *output, unsigned long fpb,
                          const struct PaStreamCallbackTimeInfo *timeinfo, PaStreamCallbackFlags flags,
                          void *userdata) {
    static char buf[ENCODE_BUFFER_SIZE];
    static size_t buflen = sizeof(buf);

    if (fpb < FRAMES_PER_BUFFER) return paContinue;
    uint16_to_bytes(buf, fpb);

    OpusEncoder *enc = source->encoder;
    opus_int32 encsize = opus_encode_float(enc, (float *)input, fpb, (unsigned char *)buf + 2, buflen - 2);
    if (encsize <= 0) {
        if (encsize < 0) fprintf(stderr, "Opus encode error: (%d) %s\n", encsize, opus_strerror(encsize));
        return paContinue;
    }

    ra_stream_t *stream = source->stream;
    ra_conn_t *conn = source->conn;
    send_crypto_data(conn, stream, buf, 2 + encsize);

    return paContinue;
}

static void send_termination_signal() {
    ra_stream_t *stream = source->stream;
    ra_conn_t *conn = source->conn;
    ra_buf_t *buf = stream->buf;

    printf("Sending stream termination signal to sink... ");
    create_terminate_message(buf);
    if (ra_stream_send(stream, conn, buf->base, buf->len) > 0) {
        printf("Sent.\n");
    } else {
        printf("Failed.\n");
    }
}

static void signal_handler(int signum) {
    is_running = false;
    if (sock >= 0) {
        send_termination_signal();
        ra_socket_close(sock);
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <sink-host> [audio-input] [sink-port]\n", argv[0]);
        return EXIT_FAILURE;
    }
    source = malloc(sizeof(ra_source_t));

    int rc = EXIT_SUCCESS;
    PaStream *pa_stream = NULL;
    OpusEncoder *encoder = NULL;
    ra_stream_t *stream = ra_stream_create(0, BUFSIZE);
    source->stream = stream;

    const char *host = argv[1];
    const char *dev = NULL;
    if (argc >= 3) {
        dev = argv[2];
    }
    int port = LISTEN_PORT;
    if (argc >= 4) {
        port = atoi(argv[3]);
    }

    // Init audio
    if (ra_audio_init()) {
        goto error;
    }
    pa_stream = ra_audio_create_stream(dev, RA_AUDIO_DEVICE_INPUT, audio_callback, NULL);
    if (!pa_stream) {
        goto error;
    }
    source->pa_stream = pa_stream;

    // Init encoder
    int err;
    encoder = opus_encoder_create(SAMPLE_RATE, CHANNELS, APPLICATION, &err);
    if (err) {
        fprintf(stderr, "Failed to create Opus encoder: (%d) %s\n", err, opus_strerror(err));
        goto error;
    }
    source->encoder = encoder;

    // Init crypto
    if (ra_crypto_init()) {
        goto error;
    }
    ra_keypair_t keypair;
    ra_generate_keypair(&keypair);
    source->keypair = &keypair;

    // Init socket
    if (ra_socket_init()) {
        goto error;
    }
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        perror("socket");
        goto error;
    }

    struct sockaddr_in client_addr;
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = INADDR_ANY;
    client_addr.sin_port = 0;

    struct sockaddr_in server_addr;
    ra_sockaddr_init(host, port, &server_addr);
    ra_conn_t sink_conn = {
        .sock = sock,
        .addr = (struct sockaddr *)&server_addr,
    };
    source->conn = &sink_conn;

    sockopt_t opt = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(sockopt_t))) {
        perror("setsockopt");
        goto error;
    }
    if (bind(sock, (struct sockaddr *)&client_addr, sizeof(struct sockaddr))) {
        perror("bind");
        goto error;
    }

    char rawbuf[BUFSIZE];
    ra_buf_t buf = {
        .base = rawbuf,
        .len = 0,
        .cap = sizeof(rawbuf),
    };
    printf("Initiating handshake with sink at %s:%d\n", host, port);
    create_handshake_message(&buf, keypair.public, sizeof(keypair.public));
    if (ra_buf_send(&sink_conn, (ra_rbuf_t *)&buf) <= 0) {
        goto error;
    }

    is_running = true;
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    struct sockaddr_in src_addr;
    socklen_t addrlen = sizeof(struct sockaddr_in);
    ra_conn_t conn = {
        .sock = sock,
        .addr = (struct sockaddr *)&src_addr,
    };
    ra_handler_context_t ctx = {
        .conn = &conn,
        .buf = (ra_rbuf_t *)&buf,
    };
    while (is_running) {
        ssize_t n = recvfrom(sock, buf.base, buf.cap, 0, (struct sockaddr *)&src_addr, &addrlen);
        if (n < 0) break;
        buf.len = n;
        handle_message(&ctx);
    }

    goto cleanup;

error:
    rc = EXIT_FAILURE;

cleanup:
    ra_stream_destroy(stream);
    if (encoder) {
        opus_encoder_destroy(encoder);
    }
    if (pa_stream) {
        Pa_StopStream(pa_stream);
        Pa_CloseStream(pa_stream);
    }
    if (sock >= 0) {
        ra_socket_close(sock);
    }
    free(source);
    ra_audio_deinit();
    ra_socket_deinit();
    printf("Source stopped gracefully.\n");
    return rc;
}
