#ifndef _RA_SOCKET_H
#define _RA_SOCKET_H

#if defined(WIN32) || defined(_WIN32)
#else
#include <arpa/inet.h>
#include <sys/socket.h>

typedef int SOCKET;
typedef int sockopt_t;
#endif

int ra_socket_init();
void ra_socket_close(SOCKET sock);
void ra_socket_deinit();

void ra_sockaddr_init(const char *host, unsigned int port, struct sockaddr_in *saddr);

#endif
