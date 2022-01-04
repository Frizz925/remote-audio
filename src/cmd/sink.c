#include "app/sink.h"

int main(int argc, char **argv) {
    ra_logger_t *logger = ra_logger_create_default();
    int rc = sink_main(logger, argc, argv);
    ra_logger_destroy(logger);
    return rc;
}
