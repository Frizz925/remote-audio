#include "socket.h"

#include <stdio.h>

static ra_logger_t *g_logger;

#ifdef _WIN32
static const char *wsa_strerror(int err) {
    static char reason[512];
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL,
                  err,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  reason,
                  sizeof(reason),
                  NULL);
    return reason;
}

static void wsa_perror(const char *msg, int err) {
    ra_logger_error(g_logger, "%s, error %d: %s", msg, err, wsa_strerror(err));
}

int ra_socket_init(ra_logger_t *logger) {
    g_logger = logger;
    WORD wVersionRequested;
    WSADATA wsaData;

    wVersionRequested = MAKEWORD(2, 2);
    int err = WSAStartup(wVersionRequested, &wsaData);
    if (err) ra_logger_fatal(logger, "WSAStartup failed, error %d: %s", err, wsa_strerror(err));
    return err;
}

void ra_socket_perror(const char *msg) {
    wsa_perror(msg, WSAGetLastError());
}

int ra_socket_select(int nfds, fd_set *fds, const struct timeval *timeout) {
    struct timeval mut_timeout = {
        .tv_sec = timeout->tv_sec,
        .tv_usec = timeout->tv_sec,
    };
    return select(nfds, fds, NULL, NULL, &mut_timeout);
}

void ra_socket_close(SOCKET sock) {
    closesocket(sock);
}

void ra_socket_deinit() {
    WSACleanup();
}

void ra_gai_perror(const char *msg, int err) {
    wsa_perror(msg, err);
}
#else
#include <netdb.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include "string.h"

static sigset_t *sigmask = NULL;

int ra_socket_init(ra_logger_t *logger) {
    g_logger = logger;
    sigmask = (sigset_t *)malloc(sizeof(sigset_t));
    sigemptyset(sigmask);
    sigaddset(sigmask, SIGINT);
    sigaddset(sigmask, SIGTERM);

    return 0;
}

int ra_socket_select(int nfds, fd_set *fds, const struct timeval *timeout) {
    struct timespec ts_timeout = {
        .tv_sec = timeout->tv_sec,
        .tv_nsec = timeout->tv_usec * 1000,
    };
    return pselect(nfds + 1, fds, NULL, NULL, &ts_timeout, sigmask);
}

void ra_socket_perror(const char *msg) {
    perror(msg);
}

void ra_socket_close(SOCKET sock) {
    close(sock);
}

void ra_socket_deinit() {
    if (sigmask) free(sigmask);
}

void ra_gai_perror(const char *msg, int err) {
    fprintf(stderr, "%s: %s\n", msg, gai_strerror(err));
}
#endif

int ra_sockaddr_init(const char *host, unsigned int port, struct sockaddr_in *saddr) {
    struct addrinfo hints = {0}, *addrinfo;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    int err = getaddrinfo(host, NULL, &hints, &addrinfo);
    if (err) {
        ra_gai_perror("getaddrinfo", err);
        return err;
    }

    memcpy(saddr, addrinfo->ai_addr, sizeof(struct sockaddr));
    saddr->sin_port = htons(port);
    freeaddrinfo(addrinfo);

    return 0;
}

void ra_sockaddr_str(char *buf, struct sockaddr_in *saddr) {
    char host[16];
    inet_ntop(saddr->sin_family, &saddr->sin_addr, host, sizeof(host));
    sprintf(buf, "%s:%d", host, ntohs(saddr->sin_port));
}
