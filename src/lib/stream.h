#ifndef _RA_STREAM_H
#define _RA_STREAM_H
#include <uv.h>

void ra_stream_default(uv_loop_t *loop, uv_tty_t *out, uv_tty_t *err);

#endif
