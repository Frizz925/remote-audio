#ifndef _RA_COMMON_H
#define _RA_COMMON_H

#include <stdbool.h>

void cleanup();
void signal_handler(int signum);
void set_running(bool running);
bool is_running();

int panic(const char *format, ...);
#endif  // _RA_COMMON_H
