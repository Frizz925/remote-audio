#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
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

#define MAX_STREAMS 16

typedef struct {
    ra_keypair_t *keypair;
    ra_audio_config_t *audio_cfg;
} ra_sink_t;

typedef struct {
    ra_stream_t *stream;
    ra_ringbuf_t *ringbuf;
    OpusDecoder *decoder;
    PaStream *pa_stream;
    uint8_t state;
    ra_audio_config_t audio_cfg;
} ra_audio_stream_t;

typedef struct {
    const ra_conn_t *conn;
    const ra_rbuf_t *buf;
} ra_handler_context_t;

static SOCKET sock = -1;
static atomic_bool is_running = false;
static ra_audio_stream_t *audio_streams[MAX_STREAMS] = {0};
static ra_sink_t *sink = NULL;

static void signal_handler(int signum) {
    is_running = true;
    if (sock >= 0) ra_socket_close(sock);
}

static void audio_stream_terminate(ra_audio_stream_t *);

static int audio_callback(const void *input, void *output, unsigned long fpb,
                          const struct PaStreamCallbackTimeInfo *timeinfo, PaStreamCallbackFlags flags,
                          void *userdata) {
    static char reason[256] = {0};
    ra_audio_stream_t *astream = userdata;
    ra_audio_config_t cfg = astream->audio_cfg;
    ra_ringbuf_t *rb = astream->ringbuf;

    if (cfg.frame_size != fpb) {
        ra_stream_t *stream = astream->stream;
        fprintf(stderr, "Stream %d: Frame size mismatch, %d != %zu\n", stream->id, cfg.frame_size, fpb);
        audio_stream_terminate(astream);
        return paAbort;
    }

    size_t sz_frame = cfg.channel_count * cfg.sample_size;
    char *wptr = (char *)output;
    char *endptr = wptr + (sz_frame * fpb);

    while (wptr != endptr) {
        const char *rptr = ra_ringbuf_read_ptr(rb);
        size_t rbytes = ra_min(ra_ringbuf_fill_count(rb), endptr - wptr);
        if (rbytes <= 0) {
            for (int i = 0; i < sz_frame; i++) *wptr++ = 0;
            continue;
        }
        memcpy(wptr, rptr, rbytes);
        ra_ringbuf_advance_read_ptr(rb, rbytes);
        wptr += rbytes;
    }

    return paContinue;
}

static ra_audio_stream_t *audio_stream_create(uint8_t id, size_t bufsize) {
    ra_audio_stream_t *astream = malloc(sizeof(ra_audio_stream_t));
    astream->ringbuf = ra_ringbuf_create(RING_BUFFER_SIZE);
    astream->stream = ra_stream_create(id, bufsize);
    astream->state = 0;
    return astream;
}

static int audio_stream_open(ra_audio_stream_t *astream, ra_audio_config_t *cfg) {
    if (astream->state == 1) return 0;

    PaStream *pa_stream = ra_audio_create_stream(cfg, audio_callback, astream);
    if (!pa_stream) {
        return -1;
    }

    int err;
    OpusDecoder *decoder = opus_decoder_create(cfg->sample_rate, cfg->channel_count, &err);
    if (err) {
        fprintf(stderr, "Failed to create Opus decoder: (%d) %s\n", err, opus_strerror(err));
        return err;
    }

    astream->state = 1;
    astream->decoder = decoder;
    astream->pa_stream = pa_stream;
    astream->audio_cfg = *cfg;

    ra_ringbuf_reset(astream->ringbuf);
    ra_stream_reset(astream->stream);
    return 0;
}

static void audio_stream_close(ra_audio_stream_t *astream) {
    if (astream->state == 0) return;

    astream->state = 0;
    opus_decoder_destroy(astream->decoder);
    Pa_StopStream(astream->pa_stream);
    Pa_CloseStream(astream->pa_stream);
}

static void audio_stream_destroy(ra_audio_stream_t *astream) {
    audio_stream_close(astream);
    ra_stream_destroy(astream->stream);
    free(astream);
}

static void audio_stream_terminate(ra_audio_stream_t *astream) {
    ra_stream_t *stream = astream->stream;
    audio_stream_close(astream);
    printf("Stream %d: Terminated\n", stream->id);
}

static void create_handshake_response_message(ra_stream_t *stream, const unsigned char *pubkey, size_t keylen) {
    ra_buf_t *buf = stream->buf;
    char *wptr = buf->base;
    *wptr++ = (char)RA_HANDSHAKE_RESPONSE;
    *wptr++ = (char)stream->id;
    *wptr++ = (char)keylen;
    memcpy(wptr, pubkey, keylen);
    buf->len = wptr - buf->base + keylen;
}

static void handle_stream_data(ra_handler_context_t *ctx, ra_audio_stream_t *astream) {
    static float pcm[DECODE_BUFFER_SIZE];

    const ra_rbuf_t *rbuf = ctx->buf;
    if (rbuf->len < 2) return;
    uint16_t fpb = bytes_to_uint16(rbuf->base);

    OpusDecoder *dec = astream->decoder;
    int samples = opus_decode_float(dec, (unsigned char *)rbuf->base + 2, rbuf->len - 2, pcm, fpb, 0);
    if (samples <= 0) {
        if (samples < 0) fprintf(stderr, "Opus decode error: (%d) %s\n", samples, opus_strerror(samples));
        return;
    } else if (fpb != samples) {
        fprintf(stderr, "Decoded sample count mismatch, %d != %d\n", fpb, samples);
        return;
    }

    ra_audio_config_t cfg = astream->audio_cfg;
    const char *rptr = (char *)pcm;
    const char *endptr = rptr + (cfg.channel_count * cfg.sample_size * samples);
    ra_ringbuf_t *rb = astream->ringbuf;
    while (rptr != endptr) {
        char *wptr = ra_ringbuf_write_ptr(rb);
        size_t wbytes = ra_min(ra_ringbuf_free_count(rb), endptr - rptr);
        if (wbytes <= 0) {
            fprintf(stderr, "Ring buffer overflow!\n");
            return;
        }
        memcpy(wptr, rptr, wbytes);
        ra_ringbuf_advance_write_ptr(rb, wbytes);
        rptr += wbytes;
    }
}

static void handle_stream_terminate(ra_handler_context_t *ctx, ra_audio_stream_t *astream) {
    audio_stream_terminate(astream);
}

static void handle_handshake_init(ra_handler_context_t *ctx) {
    const ra_rbuf_t *rbuf = ctx->buf;
    if (rbuf->len < 1) return;

    uint8_t id;
    ra_audio_stream_t **astream_ptr = NULL;
    for (id = 0; id < MAX_STREAMS; id++) {
        astream_ptr = &audio_streams[id];
        ra_audio_stream_t *astream = *astream_ptr;
        if (astream == NULL || astream->state == 0) break;
        astream_ptr = NULL;
    }
    if (astream_ptr == NULL) {
        fprintf(stderr, "Can't accept any more audio stream\n");
        return;
    } else if (*astream_ptr == NULL) {
        *astream_ptr = audio_stream_create(id, BUFSIZE);
    }
    ra_audio_stream_t *astream = *astream_ptr;
    ra_stream_t *stream = astream->stream;

    const char *rptr = rbuf->base;
    const char *endptr = rptr + rbuf->len;

    // Compute shared secret
    size_t keysize = *rptr++;
    if (rptr + keysize > endptr) return;
    const ra_keypair_t *keypair = sink->keypair;
    int err = ra_compute_shared_secret(stream->secret, sizeof(stream->secret), (unsigned char *)rptr, keysize, keypair,
                                       RA_SHARED_SECRET_SERVER);
    if (err) {
        fprintf(stderr, "Key exchange failed\n");
        return;
    }
    rptr += keysize;

    // Read audio config from handshake packet
    ra_audio_config_t cfg = *sink->audio_cfg;
    if (rptr + 8 <= endptr) {
        cfg.channel_count = *rptr++;
        cfg.sample_format = *rptr++;
        cfg.frame_size = bytes_to_uint16(rptr);
        cfg.sample_rate = bytes_to_uint32(rptr + 2);
    }

    if (audio_stream_open(astream, &cfg)) {
        fprintf(stderr, "Failed to initialize audio stream\n");
        return;
    }

    char straddr[32];
    const ra_conn_t *conn = ctx->conn;
    struct sockaddr_in *saddr = (struct sockaddr_in *)conn->addr;
    inet_ntop(saddr->sin_family, &saddr->sin_addr, straddr, sizeof(straddr));
    printf("Opened stream %d for source from %s:%d\n", stream->id, straddr, ntohs(saddr->sin_port));

    create_handshake_response_message(stream, keypair->public, sizeof(keypair->public));
    ra_buf_send(conn, (ra_rbuf_t *)stream->buf);
    Pa_StartStream(astream->pa_stream);
}

static void handle_message_crypto(ra_handler_context_t *ctx) {
    static char rawbuf[BUFSIZE];

    const ra_rbuf_t *buf = ctx->buf;
    if (buf->len < 1) return;

    // Get the stream by ID
    const char *p = buf->base;
    uint8_t stream_id = *p++;
    if (stream_id >= MAX_STREAMS) return;
    ra_audio_stream_t *astream = audio_streams[stream_id];
    if (!astream || astream->state <= 0) return;
    ra_stream_t *stream = astream->stream;

    // Read the payload
    ra_buf_t readbuf = {
        .base = rawbuf,
        .len = 0,
        .cap = sizeof(rawbuf),
    };
    if (ra_stream_read(stream, &readbuf, p, buf->len - 1)) return;

    // Prepare context data
    const char *q = rawbuf;
    ra_crypto_type crypto_type = *q++;
    ra_rbuf_t crypto_buf = {
        .base = q,
        .len = readbuf.len - 1,
    };
    ra_handler_context_t crypto_ctx = {
        .conn = ctx->conn,
        .buf = &crypto_buf,
    };

    // Handle crypto message
    switch (crypto_type) {
    case RA_STREAM_DATA:
        handle_stream_data(&crypto_ctx, astream);
        break;
    case RA_STREAM_TERMINATE:
        handle_stream_terminate(&crypto_ctx, astream);
        break;
    default:
        break;
    }
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
    case RA_HANDSHAKE_INIT:
        handle_handshake_init(&next_ctx);
        break;
    case RA_MESSAGE_CRYPTO:
        handle_message_crypto(&next_ctx);
        break;
    default:
        break;
    }
}

int main(int argc, char **argv) {
    sink = malloc(sizeof(ra_sink_t));

    int err = 0, rc = EXIT_SUCCESS;
    const char *dev = argc >= 2 ? argv[1] : NULL;

    ra_audio_config_t audio_cfg = {
        .type = RA_AUDIO_DEVICE_OUTPUT,
        .channel_count = MAX_CHANNELS,
        .frame_size = FRAMES_PER_BUFFER,
        .sample_format = 0,
        .sample_rate = 0,
    };
    sink->audio_cfg = &audio_cfg;

    if (ra_audio_init()) goto error;
    PaDeviceIndex device = ra_audio_find_device(&audio_cfg, dev);
    if (device == paNoDevice) goto error;
    printf("Using output device as sink: %s\n", ra_audio_device_name(device));

    if (ra_crypto_init()) goto error;
    ra_keypair_t keypair;
    ra_generate_keypair(&keypair);
    sink->keypair = &keypair;

    if (ra_socket_init()) goto error;
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        perror("socket");
        goto error;
    }

    sockopt_t opt = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(sockopt_t))) {
        perror("setsockopt");
        goto error;
    }

    struct sockaddr_in listen_addr;
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_addr.s_addr = INADDR_ANY;
    listen_addr.sin_port = htons(LISTEN_PORT);
    if (bind(sock, (struct sockaddr *)&listen_addr, sizeof(struct sockaddr_in))) {
        perror("bind");
        goto error;
    }
    printf("Sink listening at port %d.\n", LISTEN_PORT);

    is_running = true;
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    struct sockaddr_in src_addr;
    socklen_t addrlen = sizeof(struct sockaddr_in);

    char rawbuf[BUFSIZE];
    ra_buf_t buf = {
        .base = rawbuf,
        .len = 0,
        .cap = sizeof(rawbuf),
    };
    ra_conn_t conn = {
        .sock = sock,
        .addr = (struct sockaddr *)&src_addr,
    };
    ra_handler_context_t ctx = {
        .conn = &conn,
        .buf = (ra_rbuf_t *)&buf,
    };
    while (is_running) {
        int n = recvfrom(sock, buf.base, buf.cap, 0, (struct sockaddr *)&src_addr, &addrlen);
        if (n <= 0) break;
        buf.len = n;
        handle_message(&ctx);
    }
    printf("Sink stopped listening.\n");
    goto cleanup;

error:
    rc = EXIT_FAILURE;

cleanup:
    for (int i = 0; i < MAX_STREAMS; i++) {
        ra_audio_stream_t *astream = audio_streams[i];
        if (!astream) continue;
        audio_stream_destroy(astream);
    }
    ra_audio_deinit();
    if (sock >= 0) ra_socket_close(sock);
    ra_socket_deinit();
    free(sink);
    return rc;
}
