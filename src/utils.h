#ifndef _RA_UTILS_H
#define _RA_UTILS_H

#include <uv.h>

typedef void on_signal_cb(uv_signal_t*, int, void*);

void print_uv_error(const char *cause, int err);
void register_signals(uv_loop_t *loop, on_signal_cb *cb, void *data);

#endif
