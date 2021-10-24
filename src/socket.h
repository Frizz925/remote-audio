#ifndef _RA_SOCKET_H
#define _RA_SOCKET_H

#ifdef _WIN32
#include <fcntl.h>
#include <winsock2.h>

#define socket_close(sock) closesocket(sock);

typedef char sockopt_t;
typedef int socklen_t;
#else
#define SD_BOTH SHUT_RDWR;
#define socket_close(sock) close(sock);

typedef int SOCKET;
typedef int sockopt_t;
typedef size_t socklen_t;
#endif  // _WIN32

int socket_startup();
const char *socket_error_text();
int socket_address(char *stream, const struct sockaddr_in *addr_in);
int socket_panic(const char *message);
int socket_cleanup();
#endif  // _RA_SOCKET_H
