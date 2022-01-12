#include "app/sink.h"

#include "lib/logger.h"

int main(int argc, const char **argv) {
    ra_logger_stream_t *stream = ra_logger_stream_create_default();
    ra_logger_t *logger = ra_logger_create(stream, NULL);
    int rc = sink_main(logger, argc, argv);
    ra_logger_destroy(logger);
    ra_logger_stream_destroy(stream);
    return rc;
}
