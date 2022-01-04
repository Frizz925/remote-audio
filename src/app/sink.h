#ifndef _RA_SINK_H
#define _RA_SINK_H

#include "lib/logger.h"

int sink_main(ra_logger_t *logger, int argc, char **argv);
void sink_disable_signal_handlers();
void sink_stop();

#endif
