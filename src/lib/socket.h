#ifndef _RA_SOCKET_H
#define _RA_SOCKET_H

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

const char *straddr(const struct sockaddr *addr);

#endif
