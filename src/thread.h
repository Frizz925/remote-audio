#ifndef _RA_THREAD_H
#define _RA_THREAD_H

#ifdef _WIN32
#include <process.h>

#define RA_THREAD_CALL __cdecl

typedef uintptr_t ra_thread_t;
typedef void ra_thread_func(void *);
#else
#include <pthread.h>

#define RA_THREAD_CALL

typedef pthread_t ra_thread_t;
typedef void ra_thread_func(void *);
#endif

void ra_sleep(unsigned int seconds);

ra_thread_t ra_thread_start(ra_thread_func *routine, void *data, int *err);
void ra_thread_exit();
int ra_thread_join(ra_thread_t thread);

#endif
