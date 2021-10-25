#ifndef _RA_SOCKET_H
#define _RA_SOCKET_H

#ifdef _WIN32
#include <fcntl.h>
#include <winsock2.h>

#define socket_close(sock) closesocket(sock);

typedef char sockopt_t;
typedef int socklen_t;
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#define SD_BOTH            SHUT_RDWR
#define SOCKET_ERROR       -1
#define socket_close(sock) close(sock)

typedef int SOCKET;
typedef int sockopt_t;
#endif  // _WIN32

#define LISTEN_PORT     27100
#define MAX_PACKET_SIZE 1500
#define HEADER_SIZE     4

int socket_startup();
const char *socket_error_text();
int socket_address(char *stream, const struct sockaddr_in *addr_in);
int socket_panic(const char *message);
int socket_cleanup();
#endif  // _RA_SOCKET_H
