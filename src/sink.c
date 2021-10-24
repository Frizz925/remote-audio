#include <opus/opus.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>

#include "audio.h"
#include "common.h"
#include "ring_buffer.h"
#include "socket.h"

struct OutputContext {
    RingBuffer *ring_buffer;
    OpusDecoder *decoder;
    atomic_int exit_code;
};
typedef struct OutputContext OutputContext;

static int output_callback(const void *input,
                           void *output,
                           unsigned long frame_count,
                           const struct PaStreamCallbackTimeInfo *time_info,
                           PaStreamCallbackFlags flags,
                           void *userinfo) {
    size_t fill_bytes;
    OutputContext *ctx = (OutputContext *)userinfo;
    RingBuffer *rb = ctx->ring_buffer;
    OpusDecoder *dec = ctx->decoder;

    const unsigned char *rptr = (unsigned char *)ring_buffer_reader(rb, &fill_bytes);
    if (fill_bytes < 4) {  // Packet header not received, just fill with silence
        memset(output, 0, CHANNELS * frame_count * SAMPLE_SIZE);
        return paContinue;
    }

    int hdrlen = HEADER_SIZE;
    const short *hdr = (short *)rptr;
    const unsigned char *body = (unsigned char *)rptr + hdrlen;
    int frame_size = ntohs(hdr[0]);
    int enc_bytes = ntohs(hdr[1]);

    int result = paContinue;
    int decoded = opus_decode_float(dec, body, enc_bytes, (float *)output, frame_size, 0);
    if (decoded < 0) {
        ctx->exit_code = opus_panic("opus_decode_float", decoded);
        result = paContinue;
    } else if (decoded != frame_size) {
        fprintf(stderr, "WARNING: Frame size header and decoded frame count mismatch: %d != %d\n", frame_size, decoded);
    }
    ring_buffer_advance_reader(rb, enc_bytes + 4);

    int frames_left = frame_count - decoded;
    if (frames_left > 0) {  // Fill with silence if there are frames left
        int offset = CHANNELS * decoded * SAMPLE_SIZE;
        int bytes_left = CHANNELS * frames_left * SAMPLE_SIZE;
        memset(output + offset, 0, bytes_left);
    }

    return result;
}

static int recv_loop(SOCKET sock, PaStream *stream, OutputContext *ctx) {
    struct sockaddr_in addr_in = {0};
    socklen_t addrlen = sizeof(struct sockaddr_in);
    RingBuffer *rb = ctx->ring_buffer;

    set_running(true);
    while (is_running()) {
        int err = Pa_IsStreamActive(stream);
        if (err != 1) {
            if (err < 0) return audio_panic("audio_stream_active", err);
            break;
        }

        size_t free_bytes;
        char *wptr = ring_buffer_writer(rb, &free_bytes);
        unsigned short *hdr = (unsigned short *)wptr;
        if (free_bytes < MAX_PACKET_SIZE) {
            return panic("Ring buffer overflow\n");
        }

        int read_bytes = recvfrom(sock, wptr, MAX_PACKET_SIZE, 0, (struct sockaddr *)&addr_in, &addrlen);
        if (read_bytes <= 0) {
            break;
        } else if (read_bytes < 4) {
            fprintf(stderr, "Received packet with invalid header\n");
            continue;
        }

        int frame_size = ntohs(hdr[0]);
        int fill_bytes = ntohs(hdr[1]);
        if (fill_bytes > read_bytes) {
            fprintf(stderr, "Packet underflow detected\n");
            continue;
        } else if (frame_size != OPUS_FRAME_SIZE) {
            fprintf(stderr, "Opus frame size mismatch\n");
            continue;
        }
        ring_buffer_advance_writer(rb, read_bytes);
    }
    return ctx->exit_code;
}

int main() {
    int err, exit_code = EXIT_SUCCESS;
    char addr[32];
    SOCKET sock = SOCKET_ERROR;
    PaStream *stream = NULL;
    bool stream_started = false;

    sockopt_t val = 1;
    socklen_t addrlen = sizeof(struct sockaddr_in);

    struct sockaddr_in addr_in = {0};
    addr_in.sin_family = AF_INET;
    addr_in.sin_addr.s_addr = htonl(INADDR_ANY);
    addr_in.sin_port = htons(LISTEN_PORT);
    socket_address(addr, &addr_in);

    OutputContext ctx = {0};
    ctx.ring_buffer = ring_buffer_create(CHANNELS * OPUS_MAX_FRAME_SIZE * SAMPLE_SIZE);
    ctx.exit_code = EXIT_SUCCESS;

    exit_code = startup();
    if (exit_code) {
        goto done;
    }

    stream = audio_stream_create(NULL, AudioDeviceOutput, output_callback, &ctx, &err);
    if (!stream) {
        exit_code = (err == paNoDevice) ? panic("No output device found\n") : audio_panic("audio_stream_create", err);
        goto done;
    }

    ctx.decoder = opus_decoder_create(SAMPLE_RATE, CHANNELS, &err);
    if (!ctx.decoder) {
        exit_code = opus_panic("opus_decoder_create", err);
        goto done;
    }

    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock <= 0) {
        exit_code = socket_panic("socket");
        goto done;
    }
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(sockopt_t))) {
        exit_code = socket_panic("setsockopt");
        goto done;
    }
    if (bind(sock, (struct sockaddr *)&addr_in, addrlen)) {
        exit_code = socket_panic("bind");
        goto done;
    }
    printf("Socket bound at %s\n", addr);

    if ((err = Pa_StartStream(stream))) {
        exit_code = audio_panic("audio_stream_start", err);
        goto done;
    }
    stream_started = true;

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    exit_code = recv_loop(sock, stream, &ctx);

done:
    if (stream) {
        if (stream_started) Pa_StopStream(stream);
        Pa_CloseStream(stream);
    }
    if (ctx.decoder) opus_decoder_destroy(ctx.decoder);
    if (ctx.ring_buffer) ring_buffer_destroy(ctx.ring_buffer);
    if (sock != SOCKET_ERROR) {
        socket_close(sock);
        shutdown(sock, SD_BOTH);
    }
    cleanup();
    fprintf(stderr, "Exited gracefully\n");
    return exit_code;
}
