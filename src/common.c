#include "common.h"

#include <stdarg.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "audio.h"
#include "socket.h"

static atomic_bool _is_running = false;

int panic(const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
    return EXIT_FAILURE;
}

int startup() {
    int err;
    if ((err = socket_startup())) {
        return socket_panic("socket_startup");
    }
    if ((err = audio_init())) {
        return audio_panic("audio_init", err);
    }
    return EXIT_SUCCESS;
}

void cleanup() {
    audio_deinit();
    socket_cleanup();
}

void signal_handler(int signum) {
    if (_is_running) {
        _is_running = false;
        cleanup();
    }
}

void set_running(bool running) {
    _is_running = running;
}

bool is_running() {
    return _is_running;
}
