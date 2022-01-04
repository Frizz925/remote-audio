#ifndef _RA_SOCKET_H
#define _RA_SOCKET_H

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <WinSock2.h>
#include <WS2tcpip.h>

typedef char sockopt_t;
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

typedef int SOCKET;
typedef int sockopt_t;
#endif

int ra_socket_init();
void ra_socket_perror(const char *msg);
int ra_socket_select(int nfds, fd_set *fds, const struct timeval *timeout);
void ra_socket_close(SOCKET sock);
void ra_socket_deinit();

int ra_sockaddr_init(const char *host, unsigned int port, struct sockaddr_in *saddr);
void ra_sockaddr_str(char *buf, struct sockaddr_in *saddr);

void ra_gai_perror(const char *msg, int err);

#endif
