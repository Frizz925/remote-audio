#include "socket.h"

#include <stdio.h>

#ifdef _WIN32
static const char *wsa_strerror(int err) {
    static char reason[512];
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  reason, sizeof(reason), NULL);
    return reason;
}

int ra_socket_init() {
    WORD wVersionRequested;
    WSADATA wsaData;

    wVersionRequested = MAKEWORD(2, 2);
    int err = WSAStartup(wVersionRequested, &wsaData);
    if (err) {
        fprintf(stderr, "WSAStartup failed code %d\n", err);
        return err;
    }
    return 0;
}

void ra_socket_perror(const char *msg) {
    int err = WSAGetLastError();
    fprintf(stderr, "%s: %s", msg, wsa_strerror(err));
}

void ra_socket_close(SOCKET sock) {
    closesocket(sock);
}

void ra_socket_deinit() {
    WSACleanup();
}

void ra_gai_perror(const char *msg, int err) {
    fprintf(stderr, "%s: %d\n", msg, wsa_strerror(err));
}
#else
#include <netdb.h>
#include <unistd.h>

#include "string.h"

int ra_socket_init() {
    return 0;
}

void ra_socket_perror(const char *msg) {
    perror(msg);
}

void ra_socket_close(SOCKET sock) {
    close(sock);
}

void ra_socket_deinit() {}

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

    memcpy(saddr, addrinfo->ai_addr, sizeof(struct sockaddr_in));
    saddr->sin_port = htons(port);
    freeaddrinfo(addrinfo);

    return 0;
}
