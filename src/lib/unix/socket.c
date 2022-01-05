#include "lib/socket.h"

#include <netdb.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include "lib/string.h"

static ra_logger_t *g_logger;
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
