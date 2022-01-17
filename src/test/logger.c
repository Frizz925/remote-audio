#include "lib/logger.h"

#include <criterion/criterion.h>
#include <stdlib.h>
#include <uv.h>

Test(logger, main) {
    uv_loop_t *loop = uv_default_loop();
    ra_logger_stream_t *stream = ra_logger_stream_default(loop);
    ra_logger_t *logger = ra_logger_new(stream, "ratest");
    ra_logger_trace(logger, "This is a %-5s level log", "TRACE");
    ra_logger_debug(logger, "This is a %-5s level log", "DEBUG");
    ra_logger_info(logger, "This is a %-5s level log", "INFO");
    ra_logger_warn(logger, "This is a %-5s level log", "WARN");
    ra_logger_error(logger, "This is a %-5s level log", "ERROR");
    uv_run(loop, UV_RUN_DEFAULT);

    ra_logger_free(logger);
    ra_logger_stream_free(stream);
    uv_loop_close(loop);
}
