#include "thread.h"

#include <stdio.h>
#include <stdlib.h>

typedef struct {
    ra_thread_func *thread_func;
    void *data;
} thread_context_t;

static thread_context_t *create_thread_context(ra_thread_func *routine, void *data) {
    thread_context_t *ctx = (thread_context_t *)malloc(sizeof(thread_context_t));
    ctx->thread_func = routine;
    ctx->data = data;
    return ctx;
}

static void run_thread_context(thread_context_t *ctx) {
    ra_thread_func *fn = ctx->thread_func;
    void *data = ctx->data;
    free(ctx);
    (*fn)(data);
}

#ifdef _WIN32
#include <synchapi.h>
#include <windows.h>

static void thread_bootstrap(void *arg) {
    run_thread_context((thread_context_t *)arg);
}

void ra_sleep(unsigned int seconds) {
    Sleep(seconds * 1000);
}

ra_thread_t ra_thread_start(ra_thread_func *routine, void *data, int *err) {
    ra_thread_t handle = _beginthread(thread_bootstrap, 0, create_thread_context(routine, data));
    *err = handle == -1L ? errno : 0;
    return handle;
}

void ra_thread_exit() {
    _endthread();
}

int ra_thread_join(ra_thread_t thread) {
    return WaitForSingleObject((HANDLE)thread, INFINITE) == WAIT_FAILED;
}

int ra_thread_join_timeout(ra_thread_t thread, time_t seconds) {
    return WaitForSingleObject((HANDLE)thread, seconds * 1000);
}
#else
#include <unistd.h>

static void *thread_bootstrap(void *arg) {
    run_thread_context((thread_context_t *)arg);
    return NULL;
}

void ra_sleep(unsigned int seconds) {
    sleep(seconds);
}

ra_thread_t ra_thread_start(ra_thread_func *routine, void *data, int *err) {
    ra_thread_t thread = 0;
    *err = pthread_create((pthread_t *)&thread, NULL, &thread_bootstrap, create_thread_context(routine, data));
    return thread;
}

void ra_thread_exit() {
    pthread_exit(NULL);
}

int ra_thread_join(ra_thread_t thread) {
    return pthread_join(thread, NULL);
}
#endif
