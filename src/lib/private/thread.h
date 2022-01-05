#ifndef _RA_COMMON_THREAD_H
#define _RA_COMMON_THREAD_H

#include "lib/thread.h"

typedef struct {
    ra_thread_func *thread_func;
    void *data;
} thread_context_t;

thread_context_t *create_thread_context(ra_thread_func *routine, void *data);
void init_thread_context(thread_context_t *ctx, ra_thread_func *routine, void *data);
void run_thread_context(thread_context_t *ctx);
#endif
