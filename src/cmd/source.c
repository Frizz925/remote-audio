#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "lib/audio.h"
#include "lib/crypto.h"
#include "lib/proto.h"
#include "lib/socket.h"
#include "lib/stream.h"
#include "lib/string.h"
#include "lib/thread.h"
#include "lib/types.h"
#include "lib/utils.h"

#define HEARTBEAT_TIMEOUT_SECONDS 10

typedef struct {
    ra_conn_t *conn;
    ra_keypair_t *keypair;
    ra_stream_t *stream;
    ra_audio_config_t *audio_cfg;
    PaStream *pa_stream;
    OpusEncoder *encoder;
    atomic_uchar state;  // 0 = uninitialized, 1 = handshake sent, 2 = handshake completed
    _Atomic(time_t) last_heartbeat;
} ra_source_t;

typedef struct {
    const ra_conn_t *conn;
    const ra_rbuf_t *buf;
} ra_handler_context_t;

static SOCKET sock = -1;
static atomic_bool is_running = false;
static ra_source_t *source = NULL;

static void configure_encoder(OpusEncoder *st) {
    opus_encoder_ctl(st, OPUS_SET_SIGNAL(OPUS_SIGNAL_MUSIC));

#ifdef ENCODE_HIGH_BANDWIDTH
    opus_encoder_ctl(st, OPUS_SET_BANDWIDTH(OPUS_BANDWIDTH_FULLBAND));
    opus_encoder_ctl(st, OPUS_SET_BITRATE(OPUS_BITRATE_MAX));
    opus_encoder_ctl(st, OPUS_SET_PREDICTION_DISABLED(1));
#endif
}

static void handle_handshake_response(ra_handler_context_t *ctx) {
    if (source->state > 1) return;

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
        fprintf(stderr, "Handshake failed with the sink: key exchange failed.\n");
        return;
    }
    printf("Handshake with the sink succeed. Proceeding to stream audio to sink.\n");
    Pa_StartStream(source->pa_stream);
    source->last_heartbeat = time(NULL);
    source->state = 2;
}

static void handle_message_crypto(ra_handler_context_t *ctx) {
    static char rawbuf[BUFSIZE];

    const ra_rbuf_t *rbuf = ctx->buf;
    const char *rptr = rbuf->base;
    const char *endptr = rptr + rbuf->len;

    const uint8_t stream_id = *rptr++;
    if (stream_id != source->stream->id) return;

    ra_stream_t *stream = source->stream;
    ra_buf_t buf = {.base = rawbuf, .cap = BUFSIZE};
    if (ra_stream_read(stream, &buf, rptr, endptr - rptr)) return;
    source->last_heartbeat = time(NULL);
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
    case RA_MESSAGE_CRYPTO:
        handle_message_crypto(&next_ctx);
        break;
    default:
        break;
    }
}

static void send_crypto_data(const ra_conn_t *conn, ra_stream_t *stream, const char *src, size_t len) {
    static char rawbuf[BUFSIZE];
    static ra_buf_t buf = {.base = rawbuf, .cap = BUFSIZE};
    ra_rbuf_t rbuf = {.base = src, .len = len};
    create_stream_data_message(&buf, &rbuf);
    ra_stream_send(stream, conn, (ra_rbuf_t *)&buf);
}

static int send_handshake() {
    static char rawbuf[2048];
    static ra_buf_t buf = {.base = rawbuf, .cap = sizeof(rawbuf)};
    create_handshake_message(&buf, source->keypair, source->audio_cfg);
    if (ra_buf_sendto(source->conn, (ra_rbuf_t *)&buf) <= 0) return -1;
    source->last_heartbeat = time(NULL);
    return 0;
}

static void send_termination_signal() {
    printf("Sending stream termination signal to sink... ");
    if (ra_stream_send(source->stream, source->conn, ra_stream_terminate_message) > 0) {
        printf("Sent.\n");
    } else {
        printf("Failed.\n");
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

static void signal_handler(int signum) {
    if (sock >= 0) send_termination_signal();
    is_running = false;
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
    source->state = 0;
    source->last_heartbeat = time(NULL);

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
    ra_conn_t sink_conn = {
        .addr = (struct sockaddr *)&server_addr,
        .addrlen = sizeof(struct sockaddr_in),
    };
    source->conn = &sink_conn;

    struct sockaddr_in src_addr;
    ra_conn_t conn = {
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
    configure_encoder(encoder);
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
    ra_sockaddr_init(host, port, &server_addr);

    printf("Initiating handshake with sink at %s:%d\n", host, port);
    if (send_handshake()) goto error;
    source->state = 1;

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
            if (ra_buf_recvfrom(&conn, &buf) <= 0) goto error;
            handle_message(&ctx);
        }
        time_t now = time(NULL);
        if (source->last_heartbeat + HEARTBEAT_TIMEOUT_SECONDS <= now) {  // Heartbeat timeout
            fprintf(stderr, "Sink heartbeat timeout, re-attempting handshake.\n");
            ra_stream_reset(stream);
            if (source->state >= 2) Pa_StopStream(source->pa_stream);
            if (send_handshake()) goto error;
            source->state = 1;
        }
    }

    goto cleanup;

error:
    rc = EXIT_FAILURE;

cleanup:
    printf("Shutting down source...\n");
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

    printf("Source shutdown gracefully.\n");
    return rc;
}
