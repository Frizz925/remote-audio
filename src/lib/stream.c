#include "stream.h"

#include <stdio.h>
#include <uv.h>

void ra_stream_default(uv_loop_t *loop, uv_tty_t *out, uv_tty_t *err) {
    uv_tty_init(loop, out, STDOUT_FILENO, 0);
    uv_tty_init(loop, err, STDERR_FILENO, 0);
}
