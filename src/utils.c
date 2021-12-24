#include "utils.h"

typedef struct {
    on_signal_cb *cb;
    void *data;
} sigreg_data_t;

typedef struct {
    sigreg_data_t data;
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
    sigreg_data_t *sigdata = signal->data;
    if (sigdata->cb) {
        sigdata->cb(signal, signum, sigdata->data);
    }
    if (uv_loop_close(signal->loop)) {
        uv_walk(signal->loop, close_handle, NULL);
    }
}

void print_uv_error(const char *cause, int err) {
    fprintf(stderr, "%s %s: %s\n", cause, uv_err_name(err), uv_strerror(err));
}

void register_signals(uv_loop_t *loop, on_signal_cb *cb, void *data) {
    int sigcount = sizeof(signals) / sizeof(sigreg_t);
    for (int i = 0; i < sigcount; i++) {
        sigreg_t *reg = &signals[i];
        sigreg_data_t *sigdata = &reg->data;
        sigdata->cb = cb;
        sigdata->data = data;

        uv_signal_t *sig = &reg->handle;
        sig->data = sigdata;
        uv_signal_init(loop, sig);
        uv_signal_start(sig, on_signal, reg->signum);
    }
}
