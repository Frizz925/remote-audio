#include "sink.h"

#include <portaudio.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>

#include "lib/logger.h"
#include "lib/socket.h"

#define SINK_POOL_CAPACITY 512
#define SINK_BUFFER_SIZE   1024
#define SINK_LISTENER_PORT 21450

typedef struct {
    uv_udp_t *sock;
    ra_pool_t *pool;
    ra_logger_t *logger;
} sink_ctx_t;

typedef struct {
    size_t current;
    size_t count;
} sink_counter_t;

static void alloc_cb(uv_handle_t *req, size_t suggested_size, uv_buf_t *buf) {
    sink_ctx_t *ctx = (sink_ctx_t *)req->loop->data;
    buf->base = ra_pool_acquire(ctx->pool);
    buf->len = ra_pool_size(ctx->pool);
}

static void recv_cb(uv_udp_t *req, ssize_t nread, const uv_buf_t *buf, const struct sockaddr *addr, unsigned flags) {
    sink_ctx_t *ctx = (sink_ctx_t *)req->loop->data;
    ra_logger_t *logger = ctx->logger;
    if (nread <= 0) {
        if (nread < 0) ra_logger_error(logger, "recv_cb: %s", strerror(nread));
        goto cleanup;
    }

    char *rptr = buf->base;
    if (nread < buf->len)
        rptr[nread] = '\0';
    else
        rptr[nread - 1] = '\0';
    ra_logger_info(logger, "%s - %s", straddr(addr), rptr);

cleanup:
    ra_pool_release(ctx->pool, buf->base);
}

static void close_cb(uv_handle_t *req) {
    uv_loop_t *loop = req->loop;
    sink_counter_t *counter = (sink_counter_t *)req->data;
    counter->current++;
    if (counter->current < counter->count) return;
    uv_stop(loop);
    free(counter);
}

static void walk_cb(uv_handle_t *req, void *arg) {
    sink_counter_t *counter = (sink_counter_t *)arg;
    req->data = counter;
    counter->count++;
    uv_close(req, close_cb);
}

static void signal_cb(uv_signal_t *req, int signum) {
    uv_loop_t *loop = req->loop;
    sink_ctx_t *ctx = (sink_ctx_t *)loop->data;
    ra_logger_t *logger = ctx->logger;
    ra_logger_info(logger, "Received signal: %d", signum);

    uv_udp_recv_stop(ctx->sock);
    if (uv_loop_close(loop) == UV_EBUSY) {
        sink_counter_t *count = calloc(1, sizeof(sink_counter_t));
        uv_walk(loop, walk_cb, count);
    }
}

int sink_main(const sink_params_t *params) {
    int err;
    uv_udp_t sock;
    ra_pool_t pool;

    int rc = EXIT_SUCCESS;
    ra_logger_t *logger = params->logger;
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr = INADDR_ANY,
        .sin_port = htons(SINK_LISTENER_PORT),
    };
    sink_ctx_t ctx = {
        .sock = &sock,
        .pool = &pool,
        .logger = logger,
    };
    uv_loop_t loop = {
        .data = &ctx,
    };

    uv_loop_init(&loop);
    uv_udp_init(&loop, &sock);
    ra_pool_init(&pool, SINK_BUFFER_SIZE, SINK_POOL_CAPACITY);
    ra_logger_ref_loop(logger, &loop);

    uv_signal_t signals[] = {
        {.signum = SIGINT},
        {.signum = SIGTERM},
    };
    uv_signal_t *sig = signals;
    size_t count = sizeof(signals) / sizeof(uv_signal_t);
    for (int i = 0; i < count; i++) {
        int signum = sig->signum;
        uv_signal_init(&loop, sig);
        uv_signal_start_oneshot(sig, signal_cb, signum);
    }

    uv_udp_bind(&sock, (struct sockaddr *)&addr, UV_UDP_REUSEADDR);
    uv_udp_recv_start(&sock, alloc_cb, recv_cb);
    ra_logger_info(logger, "Sink listening at port %d.", SINK_LISTENER_PORT);

    uv_run(&loop, UV_RUN_DEFAULT);

    ra_logger_unref_loop(logger);
    ra_logger_info(logger, "Sink shutdown gracefully.");

done:
    ra_pool_deinit(&pool);
    err = uv_loop_close(&loop);
    if (err) {
        ra_logger_error(logger, "uv_loop_close: %s", uv_strerror(err));
        uv_print_all_handles(&loop, stderr);
    }
    return rc;
}
