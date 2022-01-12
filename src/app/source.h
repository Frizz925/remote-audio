#ifndef _RA_SOURCE_H
#define _RA_SOURCE_H

#include "lib/logger.h"

int source_main(ra_logger_t *logger, int argc, const char **argv);
void source_disable_signal_handlers();
void source_stop();

#endif
