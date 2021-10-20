#include <libgen.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "audio.h"
#include "socket.h"

struct SourceStreamCtx {
    struct AudioCtx *audioCtx;
    Socket sock;
};

static void source_read_callback(struct SoundIoInStream *instream, int frame_count_min, int frame_count_max) {
    int err;
    struct SoundIoChannelArea *areas;
    struct SoundIoRingBuffer *rb = instream->userdata;
    char *write_ptr = soundio_ring_buffer_write_ptr(rb);
    int free_bytes = soundio_ring_buffer_free_count(rb);
    int free_count = free_bytes / instream->bytes_per_frame;

    if (free_count < frame_count_min) {
        fprintf(stderr, "Ring buffer overflow\n");
        exit(1);
    }

    int write_frames = min_int(free_count, frame_count_max);
    int frames_left = write_frames;
    while (frames_left > 0) {
        int frame_count = frames_left;
        if ((err = soundio_instream_begin_read(instream, &areas, &frame_count))) {
            fprintf(stderr, "soundio_instream_begin_read: %s", soundio_strerror(err));
            exit(1);
        }
        if (frame_count <= 0) break;

        if (!areas) {
            // Due to an overflow there is a hole. Fill the ring buffer with
            // silence for the size of the hole.
            memset(write_ptr, 0, frame_count * instream->bytes_per_frame);
        } else {
            for (int frame = 0; frame < frame_count; frame += 1) {
                for (int ch = 0; ch < instream->layout.channel_count; ch += 1) {
                    memcpy(write_ptr, areas[ch].ptr, instream->bytes_per_sample);
                    areas[ch].ptr += areas[ch].step;
                    write_ptr += instream->bytes_per_sample;
                }
            }
        }

        if ((err = soundio_instream_end_read(instream))) {
            fprintf(stderr, "soundio_instream_end_read: %s", soundio_strerror(err));
            exit(1);
        }
        frames_left -= frame_count;
    }

    soundio_ring_buffer_advance_write_ptr(rb, write_frames * instream->bytes_per_frame);
}

static int source_sink_handle(struct SourceStreamCtx *ctx) {
    int err;
    Socket sock = ctx->sock;
    struct AudioCtx *actx = ctx->audioCtx;
    struct SoundIo *soundio = actx->soundio;
    struct SoundIoDevice *device = actx->device;
    struct SoundIoInStream *instream;
    struct SoundIoRingBuffer *rb;
    char *read_ptr;

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

    int capacity = 30 * instream->sample_rate * instream->bytes_per_frame;
    rb = soundio_ring_buffer_create(soundio, capacity);

    instream->userdata = rb;
    if ((err = soundio_instream_start(instream))) {
        fprintf(stderr, "soundio_outstream_start: %s\n", soundio_strerror(err));
        return EXIT_FAILURE;
    }

    int rc;
    for (;;) {
        soundio_flush_events(soundio);
        sleep(1);
        read_ptr = soundio_ring_buffer_read_ptr(rb);
        int fill_bytes = soundio_ring_buffer_fill_count(rb);
        int sent_bytes = send(sock, read_ptr, fill_bytes, 0);
        if (sent_bytes <= 0) {
            fprintf(stderr, "Socket closed\n");
            rc = EXIT_SUCCESS;
            break;
        } else if (sent_bytes != fill_bytes) {
            fprintf(stderr, "Socket write underflow\n");
            rc = EXIT_FAILURE;
            break;
        }
        soundio_ring_buffer_advance_read_ptr(rb, sent_bytes);
    }

    soundio_instream_destroy(instream);
    soundio_ring_buffer_destroy(rb);
    return rc;
}

static int source_handle(const char *device_name, const struct sockaddr_in *addr_in) {
    int rc;
    Socket sock;
    char addr[32];
    struct AudioCtx *actx;
    struct SourceStreamCtx ctx;
    socket_address(addr, addr_in);

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

    if (connect(sock, (struct sockaddr *)addr_in, sizeof(struct sockaddr_in))) {
        perror("connect");
        return EXIT_FAILURE;
    }
    fprintf(stderr, "Connected to %s\n", addr);

    source_sink_handle(&ctx);

    socket_close(sock);
    socket_cleanup();
    audio_context_destroy(actx);
    return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
    const char *device_name = NULL;
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <sink-host> <sink-port> [device-name]\n", basename(argv[0]));
        return EXIT_FAILURE;
    } else if (argc >= 4) {
        device_name = argv[3];
    }
    int port;
    sscanf(argv[2], "%d", &port);

    struct sockaddr_in addr_in;
    addr_in.sin_family = AF_INET;
    addr_in.sin_addr.s_addr = inet_addr(argv[1]);
    addr_in.sin_port = htons(port);

    return source_handle(device_name, &addr_in);
}