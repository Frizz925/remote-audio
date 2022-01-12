#include "lib/logger.h"

int main() {
    ra_logger_stream_t *stream = ra_logger_stream_create_default();
    ra_logger_t *logger = ra_logger_create(stream, "ratest-logger");
    ra_logger_debug(logger, "This is %s level", "debug");
    ra_logger_info(logger, "This is %s level", "info");
    ra_logger_warn(logger, "This is %s level", "warn");
    ra_logger_error(logger, "This is %s level", "error");
    // Can't test fatal since it would exit our program with failure exit code
    ra_logger_destroy(logger);
    ra_logger_stream_destroy(stream);
    return 0;
}
