#include <libgen.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audio.h"
#include "socket.h"

#define SINK_LISTEN_PORT    27100
#define SINK_LISTEN_BACKLOG 5

struct SinkStreamCtx {
    struct AudioCtx *audioCtx;
    Socket sock;
    struct sockaddr_in addr_in;
    socklen_t addrlen;
};

static void sink_write_callback(struct SoundIoOutStream *outstream, int frame_count_min, int frame_count_max) {
    int err;
    struct SoundIoChannelArea *areas;
    struct SoundIoRingBuffer *rb = outstream->userdata;
    char *dest;
    char *read_ptr = soundio_ring_buffer_read_ptr(rb);
    int fill_bytes = soundio_ring_buffer_fill_count(rb);
    int fill_count = fill_bytes / outstream->bytes_per_frame;
    int frames_left, frame_count, read_frames;
    bool rb_underflow;

    if (frame_count_min > fill_count) {
        read_frames = frame_count_min;
        rb_underflow = true;
    } else {
        read_frames = min_int(frame_count_max, fill_count);
        rb_underflow = false;
    }

    for (frames_left = read_frames; frames_left > 0; frames_left -= frame_count) {
        frame_count = frames_left;
        if ((err = soundio_outstream_begin_write(outstream, &areas, &frame_count))) {
            fprintf(stderr, "soundio_outstream_begin_write: %s\n", soundio_strerror(err));
            exit(EXIT_FAILURE);
        }
        if (frame_count <= 0) break;
        for (int i = 0; i < frame_count; i++) {
            for (int ch = 0; ch < outstream->layout.channel_count; ch++) {
                dest = areas[ch].ptr;
                if (rb_underflow) {
                    memset(dest, 0, outstream->bytes_per_sample);
                } else {
                    memcpy(dest, read_ptr, outstream->bytes_per_sample);
                    read_ptr += outstream->bytes_per_sample;
                }
                areas[ch].ptr += areas[ch].step;
            }
        }
        if ((err = soundio_outstream_end_write(outstream))) {
            fprintf(stderr, "soundio_outstream_end_write: %s\n", soundio_strerror(err));
            exit(EXIT_FAILURE);
        }
        frames_left -= frame_count;
    }

    if (!rb_underflow) soundio_ring_buffer_advance_read_ptr(rb, read_frames * outstream->bytes_per_frame);
}

static int sink_source_handle(struct SinkStreamCtx *ctx) {
    int err;
    Socket sock = ctx->sock;
    struct AudioCtx *actx = ctx->audioCtx;
    struct SoundIo *soundio = actx->soundio;
    struct SoundIoDevice *device = actx->device;
    struct SoundIoOutStream *outstream;
    struct SoundIoRingBuffer *rb;
    char *write_ptr;

    outstream = soundio_outstream_create(device);
    if (!outstream) {
        fprintf(stderr, "soundio_outsream_create: Out of memory\n");
        return EXIT_FAILURE;
    }
    outstream->format = actx->format;
    outstream->layout = actx->layout;
    outstream->sample_rate = actx->sample_rate;
    outstream->write_callback = sink_write_callback;

    if ((err = soundio_outstream_open(outstream))) {
        fprintf(stderr, "soundio_outstream_open: %s\n", soundio_strerror(err));
        return EXIT_FAILURE;
    }

    int capacity = 30 * outstream->sample_rate * outstream->bytes_per_frame;
    rb = soundio_ring_buffer_create(soundio, capacity);
    write_ptr = soundio_ring_buffer_write_ptr(rb);

    outstream->userdata = rb;
    if ((err = soundio_outstream_start(outstream))) {
        fprintf(stderr, "soundio_outstream_start: %s\n", soundio_strerror(err));
        return EXIT_FAILURE;
    }

    for (;;) {
        write_ptr = soundio_ring_buffer_write_ptr(rb);
        int free_bytes = soundio_ring_buffer_free_count(rb);
        int recv_bytes = recv(sock, write_ptr, free_bytes, 0);
        if (recv_bytes <= 0) {
            break;
        }
        soundio_ring_buffer_advance_write_ptr(rb, recv_bytes);
        soundio_flush_events(soundio);
    }

    soundio_outstream_destroy(outstream);
    soundio_ring_buffer_destroy(rb);
    return EXIT_SUCCESS;
}

static int sink_handle(const char *device_name) {
    int err;
    Socket sock;
    struct sockaddr_in addr_in;
    struct AudioCtx *actx;
    int addrlen = sizeof(struct sockaddr_in);
    char addr[32];
    char opt_c = 1;
    int opt_i = 1;

    actx = audio_context_create(device_name, AUDIO_DEVICE_OUTPUT);
    if (!actx) {
        return EXIT_FAILURE;
    }

    struct SinkStreamCtx ctx;
    ctx.audioCtx = actx;
    ctx.addrlen = addrlen;

    if (socket_startup()) {
        perror("socket_startup");
        return EXIT_FAILURE;
    }

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock <= 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

#ifdef _WIN32
    err = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt_c, sizeof(char));
#else
    err = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt_i, sizeof(int));
#endif
    if (err) {
        perror("setsockopt");
        return EXIT_FAILURE;
    }

    addr_in.sin_family = AF_INET;
    addr_in.sin_addr.s_addr = htonl(INADDR_ANY);
    addr_in.sin_port = htons(SINK_LISTEN_PORT);
    socket_address(addr, &addr_in);

    if (bind(sock, (struct sockaddr *)&addr_in, addrlen)) {
        perror("bind");
        return EXIT_FAILURE;
    }

    if (listen(sock, SINK_LISTEN_BACKLOG)) {
        perror("listen");
        return EXIT_FAILURE;
    }
    fprintf(stderr, "Sink listening at %s\n", addr);

    for (;;) {
        ctx.sock = socket_accept(sock, (struct sockaddr *)&ctx.addr_in, &ctx.addrlen);
        if (ctx.sock <= 0) {
            perror("accept");
            break;
        }
        socket_address(addr, &ctx.addr_in);
        fprintf(stderr, "Accepted source from %s\n", addr);
        sink_source_handle(&ctx);
        socket_close(ctx.sock);
        fprintf(stderr, "Closed source from %s\n", addr);
    }

    socket_close(sock);
    socket_cleanup();
    audio_context_destroy(actx);
    return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
    const char *device_name = NULL;
    if (argc >= 2) {
        device_name = argv[1];
    }
    return sink_handle(device_name);
}