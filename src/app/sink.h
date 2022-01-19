#include <uv.h>

#include "lib/logger.h"

typedef struct {
    ra_logger_t *logger;
    int argc;
    char **argv;
} sink_params_t;

int sink_main(const sink_params_t *params);
