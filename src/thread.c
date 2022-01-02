#include "thread.h"

#include <stdio.h>
#include <stdlib.h>

typedef struct {
    ra_thread_func *thread_func;
    void *data;
} thread_context_t;

#ifdef _WIN32
#include <synchapi.h>
#include <windows.h>

void ra_sleep(unsigned int seconds) {
    Sleep(seconds * 1000);
}

ra_thread_t ra_thread_start(ra_thread_func *routine, void *data, int *err) {
    ra_thread_t handle = _beginthread((_beginthread_proc_type)routine, 0, data);
    *err = handle == -1L ? errno : 0;
    return handle;
}

void ra_thread_exit() {
    _endthread();
}

int ra_thread_join(ra_thread_t thread) {
    return WaitForSingleObject((HANDLE)thread, INFINITE);
}
#else
#include <unistd.h>

static void *thread_bootstrap(void *arg) {
    thread_context_t *ctx = (thread_context_t *)arg;
    ra_thread_func *fn = ctx->thread_func;
    void *data = ctx->data;
    free(ctx);
    (*fn)(data);
    return NULL;
}

void ra_sleep(unsigned int seconds) {
    sleep(seconds);
}

ra_thread_t ra_thread_start(ra_thread_func *routine, void *data, int *err) {
    thread_context_t *ctx = (thread_context_t *)malloc(sizeof(thread_context_t));
    ctx->thread_func = routine;
    ctx->data = data;

    ra_thread_t thread = 0;
    *err = pthread_create((pthread_t *)&thread, NULL, &thread_bootstrap, ctx);
    return thread;
}

void ra_thread_exit() {
    pthread_exit(NULL);
}

int ra_thread_join(ra_thread_t thread) {
    return pthread_join(thread, NULL);
}
#endif
