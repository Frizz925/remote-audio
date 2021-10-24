#include "common.h"

#include <stdatomic.h>
#include <stdbool.h>

#include "audio.h"
#include "socket.h"

static atomic_bool _is_running = false;

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
