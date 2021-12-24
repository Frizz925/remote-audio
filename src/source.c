#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "audio.h"
#include "crypto.h"
#include "proto.h"
#include "stream.h"
#include "types.h"
#include "utils.h"

typedef struct {
    ra_keypair_t *keypair;
    ra_stream_t *stream;
    PaStream *pa_stream;
    OpusEncoder *encoder;
} ra_source_t;

typedef struct {
    ra_source_t *source;
    const ra_conn_t *conn;
} ra_callback_context_t;

typedef struct {
    ra_source_t *source;
    const ra_conn_t *conn;
    const char *buf;
    size_t len;
} ra_handler_context_t;

static void on_signal(uv_signal_t *handle, int signum, void *data) {
    printf("on_signal called\n");
    ra_source_t *source = data;
    PaStream *pa_stream = source->pa_stream;
    Pa_StopStream(pa_stream);
    Pa_CloseStream(pa_stream);
}

static void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    buf->base = malloc(suggested_size);
    buf->len = suggested_size;
}

static void create_handshake_message(uv_buf_t *buf, const unsigned char *pubkey, size_t keylen) {
    char *p = buf->base;
    *p++ = (char)RA_HANDSHAKE_INIT;
    *p++ = (char)keylen;
    memcpy(p++, pubkey, keylen);
    buf->len = p - buf->base + keylen;
}

static void send_crypto_data(const ra_conn_t *conn, ra_stream_t *stream, const char *src, size_t len) {
    char buf[65535];
    char *wptr = buf;
    *wptr++ = RA_STREAM_DATA;
    memcpy(wptr, src, len);
    ra_stream_send(stream, conn, buf, len + 1, NULL);
}

static void handle_handshake_response(ra_handler_context_t *ctx) {
    static const size_t hdrlen = 2;
    if (ctx->len < hdrlen) return;
    size_t datalen = ctx->len - hdrlen;

    ra_source_t *source = ctx->source;
    ra_stream_t *stream = source->stream;
    const char *rptr = ctx->buf;
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

static void on_read(uv_udp_t *sock, ssize_t nread, const uv_buf_t *buf, const struct sockaddr *addr, unsigned flags) {
    if (nread < 0) {
        print_uv_error("uv_udp_recv_cb", nread);
        uv_udp_recv_stop(sock);
        free(buf->base);
        return;
    }
    ra_conn_t conn = {sock, addr};

    char *p = buf->base;
    ra_message_type msg_type = (ra_message_type)*p++;
    ra_handler_context_t ctx = {sock->data, &conn, p, nread - 1};
    switch (msg_type) {
    case RA_HANDSHAKE_RESPONSE:
        handle_handshake_response(&ctx);
        break;
    default:
        break;
    }
}

static int audio_callback(const void *input, void *output, unsigned long fpb,
                          const struct PaStreamCallbackTimeInfo *timeinfo, PaStreamCallbackFlags flags,
                          void *userdata) {
    static char buf[65535];
    ra_callback_context_t *ctx = userdata;
    ra_source_t *source = ctx->source;
    ra_stream_t *stream = source->stream;
    const ra_conn_t *conn = ctx->conn;

    opus_int32 encsize = opus_encode_float(source->encoder, (float *)input, fpb, (unsigned char *)buf, sizeof(buf));
    if (encsize <= 0) return paContinue;
    printf("encsize: %d\n", encsize);
    send_crypto_data(conn, stream, buf, encsize);
    return paContinue;
}

static void print_pa_error(const char *cause, int err) {
    fprintf(stderr, "%s: (%d) %s\n", cause, err, Pa_GetErrorText(err));
}

static PaStream *init_audio_input(const char *dev, void *userdata) {
    int err = paNoError;
    const PaDeviceInfo *info;
    PaStream *stream;
    PaStreamParameters params = {0};
    params.device = paNoDevice;
    params.channelCount = CHANNELS;
    params.sampleFormat = PA_SAMPLE_TYPE;

    if (dev) {
        int count = Pa_GetDeviceCount();
        for (PaDeviceIndex index = 0; index < count; index++) {
            info = Pa_GetDeviceInfo(index);
            if (strncasecmp(dev, info->name, strlen(dev))) continue;
            if (info->maxInputChannels < CHANNELS) continue;
            params.device = index;
            err = Pa_IsFormatSupported(&params, NULL, SAMPLE_RATE);
            if (err == paFormatIsSupported) {
                break;
            }
            params.device = paNoDevice;
        }
    } else {
        params.device = Pa_GetDefaultInputDevice();
        info = Pa_GetDeviceInfo(params.device);
    }
    if (params.device == paNoDevice) {
        fprintf(stderr, "No default input device found\n");
        return NULL;
    }
    printf("Using device input for source: %s\n", info->name);

    params.suggestedLatency = info->defaultLowInputLatency;
    err = Pa_OpenStream(&stream, &params, NULL, SAMPLE_RATE, FRAMES_PER_BUFFER, 0, audio_callback, userdata);
    if (err != paNoError) {
        print_pa_error("Failed to open stream", err);
        return NULL;
    }
    return stream;
}

int main(int argc, char **argv) {
    int rc = EXIT_SUCCESS;
    PaStream *pa_stream = NULL;
    OpusEncoder *encoder = NULL;
    ra_source_t source;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <sink-host> [audio-input] [sink-port]\n", argv[0]);
        goto error;
    }
    const char *host = argv[1];
    const char *dev = NULL;
    if (argc >= 3) {
        dev = argv[2];
    }
    int port = LISTEN_PORT;
    if (argc >= 4) {
        port = atoi(argv[3]);
    }

    uv_udp_t sock = {.data = &source};
    struct sockaddr_in client_addr, server_addr;
    uv_ip4_addr(host, 0, &client_addr);
    uv_ip4_addr(host, port, &server_addr);
    ra_conn_t conn = {
        .sock = &sock,
        .addr = (struct sockaddr *)&server_addr,
    };

    if (ra_crypto_init()) {
        goto error;
    }
    ra_keypair_t keypair;
    ra_generate_keypair(&keypair);
    source.keypair = &keypair;

    if (ra_audio_init()) {
        goto error;
    }
    ra_callback_context_t cb_ctx = {.source = &source, .conn = &conn};
    pa_stream = init_audio_input(dev, &cb_ctx);
    if (!pa_stream) {
        goto error;
    }
    source.pa_stream = pa_stream;

    int err;
    encoder = opus_encoder_create(SAMPLE_RATE, CHANNELS, APPLICATION, &err);
    if (err) {
        fprintf(stderr, "Failed to create Opus encoder: (%d) %s\n", err, opus_strerror(err));
        goto error;
    }
    source.encoder = encoder;

    ra_stream_t stream;
    ra_stream_init(&stream, 0);
    source.stream = &stream;

    uv_loop_t *loop = uv_default_loop();
    uv_udp_init(loop, &sock);
    uv_udp_bind(&sock, (struct sockaddr *)&client_addr, SO_REUSEADDR);
    uv_udp_recv_start(&sock, alloc_buffer, on_read);

    printf("Initiating handshake with sink at %s:%d\n", host, port);
    create_handshake_message(&stream.buf, keypair.public, sizeof(keypair.public));
    uv_udp_send(&stream.req, &sock, &stream.buf, 1, (struct sockaddr *)&server_addr, NULL);

    register_signals(loop, &on_signal, &source);
    rc = uv_run(loop, UV_RUN_DEFAULT);
    printf("Source stopped.");
    goto cleanup;

error:
    rc = EXIT_FAILURE;

cleanup:
    if (encoder) {
        opus_encoder_destroy(encoder);
    }
    if (pa_stream) {
        Pa_StopStream(pa_stream);
        Pa_CloseStream(pa_stream);
    }
    ra_audio_deinit();
    return rc;
}
