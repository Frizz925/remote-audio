#include <libgen.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audio.h"
#include "socket.h"

struct SourceStreamCtx {
    struct AudioCtx *audioCtx;
    Socket sock;
};

static void source_read_callback(struct SoundIoInStream *instream, int frame_count_min, int frame_count_max) {
    /*
    int err;
    struct SoundIoChannelArea *areas;
    struct SoundIoRingBuffer *rb = outstream->userdata;
    char *dest;
    char *read_ptr = soundio_ring_buffer_read_ptr(rb);
    int fill_bytes = soundio_ring_buffer_fill_count(rb);
    int fill_count = fill_bytes / outstream->bytes_per_frame;
    int frames_left, frame_count;
    bool rb_underflow;

    if (frame_count_min > fill_count) {
        frames_left = frame_count_min;
        rb_underflow = true;
    } else {
        frames_left = min_int(frame_count_max, fill_count);
        rb_underflow = false;
    }

    while (frames_left > 0) {
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
    */
}

static int source_sink_handle(struct SourceStreamCtx *ctx) {
    int n, err;
    Socket sock = ctx->sock;
    struct AudioCtx *actx = ctx->audioCtx;
    struct SoundIo *soundio = actx->soundio;
    struct SoundIoDevice *device = actx->device;
    struct SoundIoInStream *instream;
    struct SoundIoRingBuffer *rb;

    instream = soundio_instream_create(device);
    if (!instream) {
        fprintf(stderr, "soundio_instream_create: Out of memory\n");
        return EXIT_FAILURE;
    }
    instream->format = actx->format;
    instream->layout = actx->layout;
    instream->sample_rate = actx->sample_rate;
    instream->read_callback = source_read_callback;

    if ((err = soundio_instream_open(instream))) {
        fprintf(stderr, "soundio_outstream_open: %s\n", soundio_strerror(err));
        return EXIT_FAILURE;
    }

    int capacity = 0.2 * instream->sample_rate * instream->bytes_per_frame;
    rb = soundio_ring_buffer_create(soundio, capacity);
    char *read_ptr = soundio_ring_buffer_read_ptr(rb);

    instream->userdata = rb;
    if ((err = soundio_instream_start(instream))) {
        fprintf(stderr, "soundio_outstream_start: %s\n", soundio_strerror(err));
        return EXIT_FAILURE;
    }

    for (;;) {
        soundio_wait_events(soundio);
        int fill_bytes = soundio_ring_buffer_fill_count(rb);
        n = send(sock, read_ptr, fill_bytes, 0);
        if (n <= 0) {
            break;
        }
    }

    soundio_instream_destroy(instream);
    soundio_ring_buffer_destroy(rb);
    return EXIT_SUCCESS;
}

static int source_handle(const char *device_name, const struct sockaddr *addr, socklen_t addrlen) {
    int rc;
    Socket sock;
    struct AudioCtx *actx;
    struct SourceStreamCtx ctx;

    actx = audio_context_create(device_name, AUDIO_DEVICE_INPUT);
    if (!actx) {
        return EXIT_FAILURE;
    }
    ctx.audioCtx = actx;

    if (socket_startup()) {
        perror("socket_startup");
        return EXIT_FAILURE;
    }

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock <= 0) {
        perror("socket");
        return EXIT_FAILURE;
    }
    ctx.sock = sock;

    if (connect(sock, addr, addrlen)) {
        perror("connect");
        return EXIT_FAILURE;
    }

    source_sink_handle(&ctx);

    socket_close(sock);
    socket_cleanup();
    audio_context_destroy(actx);
    return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <device-name> <sink-host> <sink-port>\n", basename(argv[0]));
        return EXIT_FAILURE;
    }
    int port;
    scanf(argv[3], "%d", &port);

    struct sockaddr_in addr_in;
    addr_in.sin_family = AF_INET;
    addr_in.sin_addr.s_addr = inet_addr(argv[2]);
    addr_in.sin_port = htons(port);

    return source_handle(argv[1], (struct sockaddr *)&addr_in, sizeof(struct sockaddr_in));
}