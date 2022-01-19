#include "lib/logger.h"

#include <assert.h>
#include <criterion/criterion.h>
#include <stdlib.h>
#include <uv.h>

#include "lib/stream.h"

static void test_logger_print(ra_logger_t *logger) {
    ra_logger_trace(logger, "This is a %-5s level log", "TRACE");
    ra_logger_debug(logger, "This is a %-5s level log", "DEBUG");
    ra_logger_info(logger, "This is a %-5s level log", "INFO");
    ra_logger_warn(logger, "This is a %-5s level log", "WARN");
    ra_logger_error(logger, "This is a %-5s level log", "ERROR");
}

static void test_logger_close_cb(uv_handle_t *req) {
    uv_unref(req);
}

static void test_logger_walk_cb(uv_handle_t *req, void *arg) {
    uv_close(req, test_logger_close_cb);
}

Test(logger, main) {
    uv_loop_t loop;
    uv_tty_t out, err;
    ra_logger_stream_t stream;
    ra_logger_t logger;

    uv_loop_init(&loop);
    ra_stream_default(&loop, &out, &err);
    ra_logger_stream_init(&stream, (uv_stream_t *)&out, (uv_stream_t *)&err);
    ra_logger_init(&logger, &stream, "ratest");

    test_logger_print(&logger);

    assert(uv_run(&loop, UV_RUN_DEFAULT) == 0);
    uv_loop_close(&loop);

    ra_logger_deinit(&logger);
}
