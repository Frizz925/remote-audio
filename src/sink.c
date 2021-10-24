#include <portaudio.h>
#include <signal.h>
#include <stdio.h>

#include "audio.h"
#include "common.h"
#include "socket.h"

#define LISTEN_PORT 27100
#define BUFFER_SIZE 1500

static int output_callback(const void *input,
                           void *output,
                           unsigned long frame_count,
                           const struct PaStreamCallbackTimeInfo *time_info,
                           PaStreamCallbackFlags flags,
                           void *userinfo) {
    return paContinue;
}

int main() {
    int n;
    char buf[BUFFER_SIZE];
    char addr[32];
    SOCKET sock = SOCKET_ERROR;
    AudioStream *stream = NULL;
    bool stream_started = false;

    int err = 0, exit_code = EXIT_SUCCESS;
    sockopt_t val = 1;
    socklen_t addrlen = sizeof(struct sockaddr_in);

    struct sockaddr_in addr_in = {0};
    addr_in.sin_family = AF_INET;
    addr_in.sin_addr.s_addr = htonl(INADDR_ANY);
    addr_in.sin_port = htons(LISTEN_PORT);
    socket_address(addr, &addr_in);

    if (socket_startup()) {
        exit_code = socket_panic("socket_startup");
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

    if ((err = audio_init())) {
        exit_code = audio_panic("audio_init", err);
        goto done;
    }

    stream = audio_stream_create(NULL, AudioDeviceOutput, output_callback, NULL, &err);
    if (err) {
        exit_code = audio_panic("audio_stream_create", err);
        goto done;
    }
    if ((err = audio_stream_start(stream))) {
        exit_code = audio_panic("audio_stream_start", err);
        goto done;
    }
    stream_started = true;

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    set_running(true);
    while (is_running()) {
        n = recvfrom(sock, buf, BUFFER_SIZE, 0, (struct sockaddr *)&addr_in, &addrlen);
        if (n <= 0) {
            exit_code = socket_panic("recvfrom");
            break;
        }
        buf[n] = 0;
        printf("%s\n", buf);

        err = audio_stream_active(stream);
        if (err != 1) {
            if (err < 0) exit_code = audio_panic("audio_stream_active", err);
            break;
        }
    }

done:
    if (stream) {
        if (stream_started) audio_stream_stop(stream);
        audio_stream_destroy(stream);
    }
    if (sock != SOCKET_ERROR) {
        socket_close(sock);
        shutdown(sock, SD_BOTH);
    }
    cleanup();
    fprintf(stderr, "Exited gracefully\n");
    return exit_code;
}
