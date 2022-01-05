#include "lib/private/thread.h"

#include <stdlib.h>

thread_context_t *create_thread_context(ra_thread_func *routine, void *data) {
    thread_context_t *ctx = (thread_context_t *)malloc(sizeof(thread_context_t));
    init_thread_context(ctx, routine, data);
    return ctx;
}

void init_thread_context(thread_context_t *ctx, ra_thread_func *routine, void *data) {
    ctx->thread_func = routine;
    ctx->data = data;
}

void run_thread_context(thread_context_t *ctx) {
    (*ctx->thread_func)(ctx->data);
}
