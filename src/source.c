#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "audio.h"
#include "crypto.h"
#include "proto.h"
#include "socket.h"
#include "stream.h"
#include "string.h"
#include "types.h"
#include "utils.h"

typedef struct {
    ra_conn_t *conn;
    ra_keypair_t *keypair;
    ra_stream_t *stream;
    ra_buf_t *wbuf;
    ra_audio_config_t *audio_cfg;
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

static void send_crypto_data(const ra_conn_t *conn, ra_stream_t *stream, const char *src, size_t len) {
    static char rawbuf[BUFSIZE];
    static ra_buf_t buf = {.base = rawbuf, .cap = BUFSIZE};
    ra_rbuf_t rbuf = {.base = src, .len = len};
    create_stream_data_message(&buf, &rbuf);
    ra_stream_send(stream, conn, (ra_rbuf_t *)&buf);
}

static void handle_handshake_response(ra_handler_context_t *ctx) {
    static const size_t hdrlen = 2;
    const ra_rbuf_t *rbuf = ctx->buf;
    if (rbuf->len < hdrlen) return;

    ra_stream_t *stream = source->stream;
    const char *rptr = rbuf->base;
    const char *endptr = rptr + rbuf->len;
    stream->id = (uint8_t)*rptr++;

    unsigned char keysize = (unsigned char)*rptr++;
    if (rptr + keysize > endptr) return;
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

    ra_audio_config_t *cfg = source->audio_cfg;
    if (fpb != cfg->frame_size) {
        fprintf(stderr, "Number of frames mismatch, %d != %zu\n", cfg->frame_size, fpb);
        return paAbort;
    }
    uint16_to_bytes(buf, fpb);

    OpusEncoder *enc = source->encoder;
    opus_int32 encsize = cfg->sample_format == paFloat32
                             ? opus_encode_float(enc, (float *)input, fpb, (unsigned char *)buf + 2, buflen - 2)
                             : opus_encode(enc, (opus_int16 *)input, fpb, (unsigned char *)buf + 2, buflen - 2);
    if (encsize <= 0) {
        if (encsize < 0) fprintf(stderr, "Opus encode error: (%d) %s\n", encsize, opus_strerror(encsize));
        return paAbort;
    }

    ra_stream_t *stream = source->stream;
    ra_conn_t *conn = source->conn;
    send_crypto_data(conn, stream, buf, 2 + encsize);

    return paContinue;
}

static void send_termination_signal() {
    printf("Sending stream termination signal to sink... ");
    if (ra_stream_send(source->stream, source->conn, ra_stream_terminate_message) > 0) {
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
    const char *host = argv[1];
    const char *dev = NULL;
    if (argc >= 3) dev = argv[2];
    int port = LISTEN_PORT;
    if (argc >= 4) port = atoi(argv[3]);

    source = malloc(sizeof(ra_source_t));

    int rc = EXIT_SUCCESS, err;
    PaStream *pa_stream = NULL;
    OpusEncoder *encoder = NULL;
    ra_stream_t *stream = ra_stream_create(0);
    source->stream = stream;

    char rawbuf[BUFSIZE];
    ra_buf_t buf = {
        .base = rawbuf,
        .len = 0,
        .cap = sizeof(rawbuf),
    };

    ra_keypair_t keypair;
    ra_audio_config_t audio_cfg = {
        .type = RA_AUDIO_DEVICE_INPUT,
        .channel_count = MAX_CHANNELS,
        .frame_size = FRAMES_PER_BUFFER,
        .sample_format = 0,
        .sample_rate = 0,
    };
    source->audio_cfg = &audio_cfg;

    sockopt_t opt = 1;
    struct sockaddr_in client_addr;
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = INADDR_ANY;
    client_addr.sin_port = 0;

    struct sockaddr_in server_addr;
    ra_sockaddr_init(host, port, &server_addr);
    ra_conn_t sink_conn = {
        .addr = (struct sockaddr *)&server_addr,
        .addrlen = sizeof(struct sockaddr_in),
    };
    source->conn = &sink_conn;

    struct sockaddr_in src_addr;
    ra_conn_t conn = {
        .sock = sock,
        .addr = (struct sockaddr *)&src_addr,
        .addrlen = sizeof(struct sockaddr_in),
    };
    ra_handler_context_t ctx = {
        .conn = &conn,
        .buf = (ra_rbuf_t *)&buf,
    };

    fd_set readfds;
    struct timeval select_timeout;
    select_timeout.tv_sec = 1;
    select_timeout.tv_usec = 0;

    ra_proto_init();

    // Init audio
    PaDeviceIndex device = paNoDevice;
    if (ra_audio_init()) goto error;
    device = ra_audio_find_device(&audio_cfg, dev);
    if (device == paNoDevice) goto error;
    printf("Using input device as source: %s\n", ra_audio_device_name(device));
    pa_stream = ra_audio_create_stream(&audio_cfg, audio_callback, NULL);
    if (!pa_stream) goto error;
    source->pa_stream = pa_stream;

    // Init encoder
    encoder = opus_encoder_create(audio_cfg.sample_rate, audio_cfg.channel_count, OPUS_APPLICATION, &err);
    if (err) {
        fprintf(stderr, "Failed to create Opus encoder: (%d) %s\n", err, opus_strerror(err));
        goto error;
    }
    source->encoder = encoder;

    // Init crypto
    if (ra_crypto_init()) goto error;
    ra_generate_keypair(&keypair);
    source->keypair = &keypair;

    // Init socket
    if (ra_socket_init()) goto error;
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ra_socket_perror("socket");
        goto error;
    }
    sink_conn.sock = conn.sock = sock;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(sockopt_t))) {
        ra_socket_perror("setsockopt");
        goto error;
    }
    if (bind(sock, (struct sockaddr *)&client_addr, sizeof(struct sockaddr))) {
        ra_socket_perror("bind");
        goto error;
    }

    printf("Initiating handshake with sink at %s:%d\n", host, port);
    create_handshake_message(&buf, &keypair, source->audio_cfg);
    if (ra_buf_sendto(&sink_conn, (ra_rbuf_t *)&buf) <= 0) {
        goto error;
    }

    is_running = true;
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    while (is_running) {
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);

        int count = ra_socket_select(sock + 1, &readfds, &select_timeout);
        if (count < 0) {
            ra_socket_perror("select");
            goto error;
        }
        if (FD_ISSET(sock, &readfds)) {
            if (ra_buf_recvfrom(&conn, &buf) <= 0) {
                ra_socket_perror("recvfrom");
                goto error;
            }
            handle_message(&ctx);
        }
    }

    goto cleanup;

error:
    rc = EXIT_FAILURE;

cleanup:
    ra_stream_destroy(stream);
    if (encoder) opus_encoder_destroy(encoder);
    if (pa_stream) {
        Pa_StopStream(pa_stream);
        Pa_CloseStream(pa_stream);
    }
    if (sock >= 0) ra_socket_close(sock);
    ra_socket_deinit();
    ra_audio_deinit();
    ra_proto_deinit();
    free(source);

    printf("Source stopped gracefully.\n");
    return rc;
}
