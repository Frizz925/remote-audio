#include "app/sink.h"

#include <stdio.h>
#include <uv.h>

#include "lib/logger.h"

int main(int argc, char **argv) {
    ra_logger_t logger;
    ra_logger_init(&logger, NULL, NULL);
    sink_params_t params = {
        .logger = &logger,
        .argc = argc,
        .argv = argv,
    };
    int rc = sink_main(&params);
    ra_logger_deinit(&logger);
    return rc;
}
