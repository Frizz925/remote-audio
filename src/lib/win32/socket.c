#include "lib/socket.h"

static ra_logger_t *g_logger;

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
