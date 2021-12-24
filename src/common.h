#ifndef _RA_COMMON_H
#define _RA_COMMON_H

#include <uv.h>

void print_uv_error(const char *cause, int err);
void register_signals(uv_loop_t *loop);

#endif
