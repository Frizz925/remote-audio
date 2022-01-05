#include "lib/private/thread.h"

#include <time.h>
#include <unistd.h>

struct ra_thread_handle_t {
    pthread_t thread;
    pthread_mutex_t mutex;
    pthread_mutexattr_t mutex_attr;
};

typedef struct {
    thread_context_t base;
    pthread_mutex_t *mutex;
} thread_context_mutex_t;

static void thread_handle_cleanup(ra_thread_t handle) {}

static void *thread_bootstrap(void *arg) {
    thread_context_mutex_t *ctx = (thread_context_mutex_t *)arg;
    pthread_mutex_lock(ctx->mutex);
    run_thread_context((thread_context_t *)ctx);
    pthread_mutex_unlock(ctx->mutex);
    free(ctx);

    pthread_exit(NULL);
    return NULL;
}

#ifdef __APPLE__
static int pthread_mutex_timedlock(pthread_mutex_t *mutex, const struct timespec *abs_timeout) {
    while (time(NULL) < abs_timeout->tv_sec) {
        if (pthread_mutex_trylock(mutex))
            sleep(1);
        else
            return 0;
    }
    return ETIMEDOUT;
}
#endif

void ra_sleep(unsigned int seconds) {
    sleep(seconds);
}

ra_thread_t ra_thread_start(ra_thread_func *routine, void *data, int *err) {
    ra_thread_handle_t *handle = (ra_thread_handle_t *)malloc(sizeof(ra_thread_handle_t));
    thread_context_mutex_t *ctx = (thread_context_mutex_t *)malloc(sizeof(thread_context_mutex_t));
    ctx->mutex = &handle->mutex;

    *err = pthread_mutexattr_init(&handle->mutex_attr);
    if (*err) goto error;
    *err = pthread_mutexattr_settype(&handle->mutex_attr, PTHREAD_MUTEX_ERRORCHECK);
    if (*err) goto error;
    *err = pthread_mutex_init(&handle->mutex, &handle->mutex_attr);
    if (*err) goto error;

    init_thread_context((thread_context_t *)ctx, routine, data);
    *err = pthread_create(&handle->thread, NULL, &thread_bootstrap, ctx);
    if (*err) goto error;

    return handle;

error:
    free(ctx);
    ra_thread_destroy(handle);
    return 0;
}

int ra_thread_join(ra_thread_t handle) {
    return pthread_join(handle->thread, NULL);
}

int ra_thread_join_timeout(ra_thread_t handle, time_t seconds) {
    struct timespec abs_timeout = {
        .tv_sec = time(NULL) + seconds,
        .tv_nsec = 0,
    };
    int res = pthread_mutex_timedlock(&handle->mutex, &abs_timeout);
    if (res) return res;
    return pthread_mutex_unlock(&handle->mutex);
}

int ra_thread_destroy(ra_thread_t handle) {
    pthread_mutex_destroy(&handle->mutex);
    pthread_mutexattr_destroy(&handle->mutex_attr);
    pthread_detach(handle->thread);
    free(handle);
    return 0;
}
