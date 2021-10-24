#include "socket.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32

static atomic_bool socket_initialized = false;
static char *socket_error_messages[12000] = {0};

int socket_startup() {
    WSADATA wsaData;
    int err = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (err == SOCKET_ERROR) return WSAGetLastError();
    socket_initialized = true;
    return 0;
}

const char *socket_error_text() {
    int err = WSAGetLastError();
    char *msg = socket_error_messages[err % 12000];
    if (!msg) {
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                      NULL,
                      err,
                      0,
                      (LPSTR)&msg,
                      0,
                      NULL);
    }
    return msg;
}

int socket_panic(const char *message) {
    fprintf(stderr, "%s: %s", message, socket_error_text());
    return EXIT_FAILURE;
}

int socket_cleanup() {
    int err = WSACleanup();
    if (err == SOCKET_ERROR) return WSAGetLastError();
    socket_initialized = false;
    return 0;
}
#else
#include <errno.h>
#include <string.h>

int socket_startup() {
    return 0;
}

const char *socket_error_text() {
    return strerror(errno);
}

int socket_panic(const char *message) {
    perror(message);
    return EXIT_FAILURE;
}

int socket_cleanup() {
    return 0;
}
#endif

int socket_address(char *stream, const struct sockaddr_in *addr_in) {
    return sprintf(stream, "%s:%d", inet_ntoa(addr_in->sin_addr), ntohs(addr_in->sin_port));
}
