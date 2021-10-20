#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "audio.h"
#include "socket.h"

#define SINK_LISTEN_PORT    27100
#define SINK_LISTEN_BACKLOG 5

struct SourceStreamCtx {
    struct AudioCtx *audioCtx;
    Socket sock;
    struct sockaddr_in addr_in;
    socklen_t addrlen;
};

static void write_callback(struct SoundIoOutStream *outstream, int frame_count_min, int frame_count_max) {
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
}

static int source_handle(struct SourceStreamCtx *ctx) {
    int n, err;
    Socket sock = ctx->sock;
    struct AudioCtx *actx = ctx->audioCtx;
    struct SoundIo *soundio = actx->soundio;
    struct SoundIoDevice *device = actx->device;
    struct SoundIoOutStream *outstream;
    struct SoundIoRingBuffer *rb;

    outstream = soundio_outstream_create(device);
    if (!outstream) {
        fprintf(stderr, "soundio_oustream_create: Out of memory\n");
        return EXIT_FAILURE;
    }
    outstream->format = actx->format;
    outstream->layout = actx->layout;
    outstream->sample_rate = actx->sample_rate;
    outstream->write_callback = write_callback;
    outstream->userdata = rb;

    if ((err = soundio_outstream_open(outstream))) {
        fprintf(stderr, "soundio_outstream_open: %s\n", soundio_strerror(err));
        return EXIT_FAILURE;
    }

    int capacity = 0.2 * outstream->sample_rate * outstream->bytes_per_frame;
    rb = soundio_ring_buffer_create(soundio, capacity);
    char *write_ptr = soundio_ring_buffer_write_ptr(rb);
    int free_bytes = capacity / 2;
    memset(write_ptr, 0, free_bytes);
    soundio_ring_buffer_advance_write_ptr(rb, free_bytes);

    if ((err = soundio_outstream_start(outstream))) {
        fprintf(stderr, "soundio_outstream_start: %s\n", soundio_strerror(err));
        return EXIT_FAILURE;
    }

    for (;;) {
        free_bytes = soundio_ring_buffer_free_count(rb);
        n = recv(sock, write_ptr, free_bytes, 0);
        if (n <= 0) {
            break;
        }
        soundio_wait_events(soundio);
    }

    soundio_outstream_destroy(outstream);
    soundio_ring_buffer_destroy(rb);
    return EXIT_SUCCESS;
}

static int server_handle(struct AudioCtx *actx) {
    int err;
    Socket sock;
    struct sockaddr_in addr_in;
    int addrlen = sizeof(struct sockaddr_in);
    char addr[32];
    char opt_c = 1;
    int opt_i = 1;

    struct SourceStreamCtx ctx;
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
            return EXIT_FAILURE;
        }
        socket_address(addr, &ctx.addr_in);
        fprintf(stderr, "Accepted source from %s\n", addr);
        source_handle(&ctx);
        socket_close(ctx.sock);
        fprintf(stderr, "Closed source from %s\n", addr);
    }
}

static int sink_handle(const char *device_name) {
    int rc, err;
    struct SoundIo *soundio;
    struct SoundIoDevice *device;
    struct SoundIoChannelLayout layout;
    enum SoundIoFormat fmt = AUDIO_FORMAT;
    int ar = AUDIO_SAMPLE_RATE, ac = AUDIO_CHANNELS;

    soundio = soundio_create();
    if (!soundio) {
        fprintf(stderr, "soundio_create: Out of memory\n");
        return EXIT_FAILURE;
    }

    if ((err = soundio_connect(soundio))) {
        fprintf(stderr, "soundio_connect: %s\n", soundio_strerror(err));
        return EXIT_FAILURE;
    }
    soundio_flush_events(soundio);

    device = NULL;
    for (int i = 0; i < soundio_output_device_count(soundio); i++) {
        device = soundio_get_output_device(soundio, i);
        if (!strncasecmp(device_name, device->name, strlen(device_name))) {
            break;
        }
        soundio_device_unref(device);
        device = NULL;
    }
    if (!device) {
        fprintf(stderr, "Output device not found: %s\n", device_name);
        return EXIT_FAILURE;
    }
    if (!soundio_device_supports_format(device, fmt)) {
        fprintf(stderr, "Output device format unsupported: %s\n", soundio_format_string(fmt));
        return EXIT_FAILURE;
    }
    if (!soundio_device_supports_sample_rate(device, ar)) {
        fprintf(stderr, "Output device sample rate unsupported: %d\n", ar);
        return EXIT_FAILURE;
    }

    struct SoundIoChannelLayout *layout_ptr = NULL;
    for (int i = 0; i < device->layout_count; i++) {
        layout_ptr = &device->layouts[i];
        if (layout_ptr->channel_count == ac) {
            break;
        }
        layout_ptr = NULL;
    }
    if (!layout_ptr) {
        fprintf(stderr, "Output device layouts unsupported\n");
        return EXIT_FAILURE;
    }
    layout = *layout_ptr;

    struct AudioCtx ctx;
    ctx.soundio = soundio;
    ctx.device = device;
    ctx.layout = layout;
    ctx.format = fmt;
    ctx.sample_rate = ar;
    rc = server_handle(&ctx);

    soundio_device_unref(device);
    soundio_destroy(soundio);
    return rc;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <device-name>\n", basename(argv[0]));
        return EXIT_FAILURE;
    }
    return sink_handle(argv[1]);
}