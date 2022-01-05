#include "lib/private/thread.h"

#include <synchapi.h>

static void thread_bootstrap(void *arg) {
    run_thread_context((thread_context_t *)arg);
}

void ra_sleep(unsigned int seconds) {
    Sleep(seconds * 1000);
}

ra_thread_t ra_thread_start(ra_thread_func *routine, void *data, int *err) {
    uintptr_t handle = _beginthread(thread_bootstrap, 0, create_thread_context(routine, data));
    *err = handle == -1L ? errno : 0;
    return (HANDLE)handle;
}

void ra_thread_exit() {
    _endthread();
}

int ra_thread_join(ra_thread_t thread) {
    return WaitForSingleObject(thread, INFINITE) == WAIT_FAILED;
}

int ra_thread_join_timeout(ra_thread_t thread, time_t seconds) {
    return WaitForSingleObject(thread, seconds * 1000);
}

int ra_thread_destroy(ra_thread_t thread) {
    return !CloseHandle(thread);
}
