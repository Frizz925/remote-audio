#include "common.h"

typedef struct {
    uv_signal_t handle;
    int signum;
} sigreg_t;

static sigreg_t signals[] = {
    {.signum = SIGINT},
    {.signum = SIGTERM},
};

static void close_handle(uv_handle_t *handle, void *arg) {
    uv_close(handle, NULL);
}

static void on_signal(uv_signal_t *signal, int signum) {
    fprintf(stderr, "Received signal: %d\n", signum);
    if (uv_loop_close(signal->loop)) {
        uv_walk(signal->loop, close_handle, NULL);
    }
}

void print_uv_error(const char *cause, int err) {
    fprintf(stderr, "%s %s: %s\n", cause, uv_err_name(err), uv_strerror(err));
}

void register_signals(uv_loop_t *loop) {
    int sigcount = sizeof(signals) / sizeof(sigreg_t);
    for (int i = 0; i < sigcount; i++) {
        sigreg_t *reg = &signals[i];
        uv_signal_t *sig = &reg->handle;
        uv_signal_init(loop, sig);
        uv_signal_start(sig, on_signal, reg->signum);
    }
}
