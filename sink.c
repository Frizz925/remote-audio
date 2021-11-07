#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <sodium.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#include "proto.h"
#include "socket.h"

#define LISTENER_PORT 12040

static int sock = SOCKET_NONE;
static atomic_bool is_running = false;
static unsigned char privkey[crypto_aead_xchacha20poly1305_IETF_KEYBYTES];
static unsigned char pubkey[crypto_scalarmult_BYTES];

static void handle_signal() {
    is_running = false;
    if (sock > 0) close(sock);
    sock = SOCKET_NONE;
}

static int handle_handshake_init(const char *in, char *out, struct sockaddr_in *addr_in) {
    char addr[24];
    addrstr(addr, addr_in);
    printf("Handshake from %s\n", addr);

    unsigned char *peerkey;
    keylen_t keylen;
    proto_read_handshake_init(in, &peerkey, &keylen);

    return 0;
}

int main() {
    int err = 0, rc = EXIT_SUCCESS;

    if (sodium_init()) {
        fprintf(stderr, "Failed to initialize sodium\n");
        return EXIT_FAILURE;
    }
    crypto_aead_xchacha20poly1305_ietf_keygen(privkey);
    crypto_scalarmult_base(pubkey, privkey);

    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock <= 0) {
        perror("socket");
        goto cleanup;
    }

    int opt = 1;
    err = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int));
    if (err) {
        perror("setsockopt");
        goto cleanup;
    }

    struct sockaddr_in addr_in = {0};
    addr_in.sin_family = AF_INET;
    addr_in.sin_addr.s_addr = htonl(INADDR_ANY);
    addr_in.sin_port = htons(LISTENER_PORT);
    socklen_t addrlen = sizeof(struct sockaddr_in);

    err = bind(sock, (struct sockaddr *)&addr_in, addrlen);
    if (err) {
        perror("bind");
        goto cleanup;
    }

    is_running = true;
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    char in[PACKET_SIZE], out[PACKET_SIZE];
    char *bp;
    int n, type, flags;
    while (is_running) {
        bp = in;
        n = recvfrom(sock, in, PACKET_SIZE, 0, (struct sockaddr *)&addr_in, &addrlen);
        if (n <= 0) {
            perror("recvfrom");
            rc = EXIT_FAILURE;
            break;
        }
        bp += proto_read_header(bp, &type, &flags);
        switch (type) {
            case PacketTypeHandshake:
                if (flags == PacketFlagSYN) {
                    n = handle_handshake_init(bp, out, &addr_in);
                    if (n > 0) sendto(sock, out, n, 0, (struct sockaddr *)&addr_in, addrlen);
                }
                break;
        }
    }

cleanup:
    if (sock > 0) {
        close(sock);
    } else {
        rc = EXIT_FAILURE;
    }
    if (err) rc = EXIT_FAILURE;
    fprintf(stderr, "Exited gracefully\n");
    return rc;
}
