#ifndef _RA_COMMON_H
#define _RA_COMMON_H

#include <stdbool.h>

#define LISTEN_PORT     27100
#define MAX_PACKET_SIZE 1500
#define HEADER_SIZE     4

int panic(const char *format, ...);

int startup();
void cleanup();
void signal_handler(int signum);
void set_running(bool running);
bool is_running();
#endif  // _RA_COMMON_H
