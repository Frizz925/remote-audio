#include <libgen.h>
#include <opus/opus.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "audio.h"
#include "common.h"
#include "socket.h"

static int send_loop(SOCKET sock, PaStream *stream, OpusEncoder *enc, const struct sockaddr_in *addr_in) {
    int err, rc = EXIT_SUCCESS;
    size_t addrlen = sizeof(struct sockaddr_in);

    int buflen = CHANNELS * OPUS_FRAME_SIZE * SAMPLE_SIZE;
    char buf[2 * buflen];
    char packet[MAX_PACKET_SIZE];

    int hdrlen = HEADER_SIZE;
    int bodylen = MAX_PACKET_SIZE - hdrlen;
    short *hdr = (short *)packet;
    unsigned char *body = (unsigned char *)packet + hdrlen;
    hdr[0] = htons(OPUS_FRAME_SIZE);

    set_running(true);
    while (is_running()) {
        if ((err = Pa_ReadStream(stream, buf, OPUS_FRAME_SIZE))) {
            rc = Pa_Panic("Pa_ReadStream", err);
            break;
        }
        int enc_bytes = opus_encode_float(enc, (float *)buf, OPUS_FRAME_SIZE, body, bodylen);
        hdr[1] = htons(enc_bytes);
        int write_bytes = sendto(sock, packet, hdrlen + enc_bytes, 0, (struct sockaddr *)addr_in, addrlen);
        if (write_bytes < enc_bytes) {
            fprintf(stderr, "WARNING: Packet underflow\n");
        }
    }

    return rc;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        return panic("Usage: %s <sink-host> [sink-port] [input-device]", basename(argv[0]));
    }

    int err, exit_code = EXIT_SUCCESS;
    SOCKET sock = SOCKET_ERROR;
    const char *device_name = NULL;
    const char *sink_host = argv[1];
    int sink_port = LISTEN_PORT;
    char addr[32];

    PaStream *stream = NULL;
    OpusEncoder *enc = NULL;
    bool stream_started = false;

    if (argc >= 3) sscanf(argv[2], "%d", &sink_port);
    if (argc >= 4) device_name = argv[3];

    struct sockaddr_in addr_in = {0};
    addr_in.sin_family = AF_INET;
    addr_in.sin_addr.s_addr = inet_addr(sink_host);
    addr_in.sin_port = htons(sink_port);
    socket_address(addr, &addr_in);

    exit_code = startup();
    if (exit_code) {
        goto done;
    }

    stream = audio_stream_create(device_name, AudioDeviceInput, NULL, NULL, &err);
    if (!stream) {
        exit_code = audio_panic("audio_stream_create", err);
        goto done;
    }

    enc = opus_encoder_create(SAMPLE_RATE, CHANNELS, OPUS_APPLICATION, &err);
    if (!enc) {
        exit_code = opus_panic("opus_encoder_create", err);
        goto done;
    }

    if ((err = Pa_StartStream(stream))) {
        goto done;
    }
    stream_started = true;

    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock <= 0) {
        exit_code = socket_panic("socket");
        goto done;
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    printf("Streaming audio to sink at %s\n", addr);
    exit_code = send_loop(sock, stream, enc, &addr_in);

done:
    if (stream) {
        if (stream_started) Pa_StopStream(stream);
        Pa_CloseStream(stream);
    }
    if (enc) opus_encoder_destroy(enc);
    if (sock != SOCKET_ERROR) {
        socket_close(sock);
        shutdown(sock, SD_BOTH);
    }
    cleanup();
    fprintf(stderr, "Exited gracefully\n");
    return exit_code;
}
