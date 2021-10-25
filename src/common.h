#ifndef _RA_COMMON_H
#define _RA_COMMON_H

#include <stdbool.h>

int panic(const char *format, ...);

int startup();
void cleanup();
void signal_handler(int signum);
void set_running(bool running);
bool is_running();
#endif  // _RA_COMMON_H
