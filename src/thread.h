#ifndef _RA_THREAD_H
#define _RA_THREAD_H

#ifdef _WIN32
#include <process.h>

typedef uintptr_t ra_thread_t;
#else
#include <pthread.h>

typedef pthread_t ra_thread_t;
#endif

typedef void ra_thread_func(void *);

void ra_sleep(unsigned int seconds);

ra_thread_t ra_thread_start(ra_thread_func *routine, void *data, int *err);
void ra_thread_exit();
int ra_thread_join(ra_thread_t thread);

#endif
