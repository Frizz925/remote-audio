#include <errno.h>
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
#include "thread.h"
#include "types.h"
#include "utils.h"

#define MAX_STREAMS 16
#define LIVENESS_TIMEOUT_SECONDS 10
#define HEARTBEAT_INTERVAL_SECONDS 3

typedef struct {
    ra_keypair_t *keypair;
    ra_audio_config_t *audio_cfg;
} ra_sink_t;

typedef struct {
    ra_stream_t *stream;
    ra_ringbuf_t *ringbuf;
    OpusDecoder *decoder;
    PaStream *pa_stream;
    atomic_uchar state;
    ra_audio_config_t audio_cfg;
    ra_conn_t conn;
    struct sockaddr_in _addr;
    _Atomic(time_t) last_update;
    _Atomic(time_t) last_heartbeat;
} ra_audio_stream_t;

typedef struct {
    const ra_conn_t *conn;
    const ra_rbuf_t *buf;
} ra_handler_context_t;

static SOCKET sock = -1;
static atomic_bool is_running = false;
static ra_audio_stream_t *audio_streams[MAX_STREAMS] = {0};
static ra_sink_t *sink = NULL;

static void audio_stream_close(ra_audio_stream_t *);

static void signal_handler(int signum) {
    is_running = false;
}

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
        audio_stream_close(astream);
        return paAbort;
    }

    size_t sz_frame = cfg.channel_count * cfg.sample_size;
    size_t sz_buffer = sz_frame * fpb;
    char *wptr = (char *)output;
    char *endptr = wptr + (sz_frame * fpb);

    while (wptr < endptr) {
        const char *rptr = ra_ringbuf_read_ptr(rb);
        size_t rbytes = ra_min(ra_ringbuf_fill_count(rb), endptr - wptr);
        if (rbytes < sz_buffer) {
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
    astream->stream = ra_stream_create(id);
    astream->state = 0;
    astream->conn.sock = -1;
    astream->conn.addr = (struct sockaddr *)&astream->_addr;
    astream->conn.addrlen = sizeof(astream->_addr);
    return astream;
}

static int audio_stream_open(ra_audio_stream_t *astream, ra_audio_config_t *cfg, const ra_conn_t *conn) {
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
    astream->conn.sock = conn->sock;
    memcpy(&astream->_addr, conn->addr, conn->addrlen);
    astream->last_update = time(NULL);

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

static void send_handshake_response(ra_audio_stream_t *astream, const ra_keypair_t *keypair) {
    char rawbuf[BUFSIZE];
    ra_buf_t buf = {
        .base = rawbuf,
        .cap = sizeof(rawbuf),
    };
    create_handshake_response_message(&buf, astream->stream->id, keypair);
    ra_buf_sendto(&astream->conn, (ra_rbuf_t *)&buf);
}

static void send_stream_signal(ra_audio_stream_t *astream, const ra_rbuf_t *message) {
    ra_stream_send(astream->stream, &astream->conn, message);
}

static void send_stream_heartbeat(ra_audio_stream_t *astream) {
    send_stream_signal(astream, ra_stream_heartbeat_message);
    astream->last_heartbeat = time(NULL);
}

static void send_stream_terminate(ra_audio_stream_t *astream) {
    send_stream_signal(astream, ra_stream_terminate_message);
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
    while (rptr < endptr) {
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
    audio_stream_close(astream);
    printf("Stream %d: Terminated\n", astream->stream->id);
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

    const ra_conn_t *conn = ctx->conn;
    if (audio_stream_open(astream, &cfg, conn)) {
        fprintf(stderr, "Failed to initialize audio stream\n");
        return;
    }

    static char straddr[32];
    ra_sockaddr_str(straddr, (struct sockaddr_in *)conn->addr);
    printf("Opened stream %d for source from %s\n", stream->id, straddr);
    send_handshake_response(astream, keypair);
    Pa_StartStream(astream->pa_stream);
}

static void handle_message_crypto(ra_handler_context_t *ctx) {
    static char rawbuf[BUFSIZE];

    const ra_rbuf_t *rbuf = ctx->buf;
    if (rbuf->len < 1) return;

    // Get the stream by ID
    const char *rptr = rbuf->base;
    const char *endptr = rptr + rbuf->len;

    uint8_t stream_id = *rptr++;
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
    if (ra_stream_read(stream, &readbuf, rptr, endptr - rptr)) return;
    astream->last_update = time(NULL);

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
    const ra_rbuf_t *rbuf = ctx->buf;
    const char *rptr = rbuf->base;
    ra_message_type msg_type = (ra_message_type)*rptr++;

    ra_rbuf_t next_buf = {
        .base = rptr,
        .len = rbuf->len - 1,
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

static void handle_liveness() {
    time_t now = time(NULL);
    for (int i = 0; i < MAX_STREAMS; i++) {
        ra_audio_stream_t *astream = audio_streams[i];
        if (!astream || astream->state <= 0) continue;
        ra_stream_t *stream = astream->stream;
        if (astream->last_update + LIVENESS_TIMEOUT_SECONDS <= now) {
            audio_stream_close(astream);
            send_stream_terminate(astream);
            printf("Stream %d: Terminated due to liveness timeout\n", stream->id);
            break;
        }
        if (astream->last_heartbeat + HEARTBEAT_INTERVAL_SECONDS <= now) {
            send_stream_heartbeat(astream);
        }
    }
}

static void background_thread(void *arg) {
    printf("Background thread started.\n");
    while (is_running) {
        handle_liveness();
        ra_sleep(1);
    }
    printf("Background thread stopped.\n");
}

int main(int argc, char **argv) {
    sink = (ra_sink_t *)malloc(sizeof(ra_sink_t));
    int err = 0, rc = EXIT_SUCCESS;
    const char *dev = argc >= 2 ? argv[1] : NULL;

    char rawbuf[BUFSIZE];
    ra_buf_t buf = {
        .base = rawbuf,
        .len = 0,
        .cap = sizeof(rawbuf),
    };

    ra_keypair_t keypair;
    sink->keypair = &keypair;

    ra_audio_config_t audio_cfg = {
        .type = RA_AUDIO_DEVICE_OUTPUT,
        .channel_count = MAX_CHANNELS,
        .frame_size = FRAMES_PER_BUFFER,
        .sample_format = 0,
        .sample_rate = 0,
    };
    sink->audio_cfg = &audio_cfg;

    struct sockaddr_in listen_addr;
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_addr.s_addr = INADDR_ANY;
    listen_addr.sin_port = htons(LISTEN_PORT);
    sockopt_t opt = 1;

    struct sockaddr_in src_addr;
    ra_conn_t conn = {
        .sock = -1,
        .addr = (struct sockaddr *)&src_addr,
        .addrlen = sizeof(struct sockaddr_in),
    };
    ra_handler_context_t ctx = {
        .conn = &conn,
        .buf = (ra_rbuf_t *)&buf,
    };
    ra_thread_t thread = 0;

    fd_set readfds;
    struct timeval select_timeout;
    select_timeout.tv_sec = 1;
    select_timeout.tv_usec = 0;

    ra_proto_init();

    PaDeviceIndex device = paNoDevice;
    if (ra_audio_init()) goto error;
    device = ra_audio_find_device(&audio_cfg, dev);
    if (device == paNoDevice) goto error;
    printf("Using output device as sink: %s\n", ra_audio_device_name(device));

    if (ra_crypto_init()) goto error;
    ra_generate_keypair(&keypair);

    if (ra_socket_init()) goto error;
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ra_socket_perror("socket");
        goto error;
    }
    conn.sock = sock;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(sockopt_t))) {
        ra_socket_perror("setsockopt");
        goto error;
    }
    if (bind(sock, (struct sockaddr *)&listen_addr, sizeof(struct sockaddr_in))) {
        ra_socket_perror("bind");
        goto error;
    }
    printf("Sink listening at port %d.\n", LISTEN_PORT);

    is_running = true;
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    thread = ra_thread_start(&background_thread, NULL, &err);
    if (err) {
        perror("thread_start");
        goto error;
    }

    while (is_running) {
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        int count = ra_socket_select(sock + 1, &readfds, &select_timeout);
        if (count < 0) {
            ra_socket_perror("select");
            goto error;
        }
        if (count == 0 || !FD_ISSET(sock, &readfds)) continue;
        if (ra_buf_recvfrom(&conn, &buf) <= 0) goto error;
        handle_message(&ctx);
    }
    goto cleanup;

error:
    rc = EXIT_FAILURE;

cleanup:
    printf("Sink shutting down...\n");
    if (thread) {
        if (ra_thread_join_timeout(thread, 30) == RA_THREAD_WAIT_TIMEOUT)
            fprintf(stderr, "Timeout waiting for background thread to stop\n");
        ra_thread_destroy(thread);
    }
    for (int i = 0; i < MAX_STREAMS; i++) {
        ra_audio_stream_t *astream = audio_streams[i];
        if (!astream) continue;
        audio_stream_destroy(astream);
    }
    if (sock >= 0) ra_socket_close(sock);
    ra_socket_deinit();
    ra_audio_deinit();
    ra_proto_deinit();
    free(sink);

    printf("Sink shutdown gracefully.\n");
    return rc;
}
