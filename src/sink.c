#include <arpa/inet.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "crypto.h"
#include "message.h"
#include "stream.h"
#include "types.h"
#include "utils.h"

#define LISTENER_PORT 1204
#define MAX_MESSAGE_SIZE 1200
#define MAX_STREAMS 128

static int sock_fd = -1;
static atomic_bool is_running = false;
static uint8 next_stream_id = 0;
static Stream *streams[MAX_STREAMS] = {0};

static void signal_handler(int signum) {
    fprintf(stderr, "Received signal: %s\n", strsignal(signum));
    is_running = false;
    if (sock_fd > 0) close(sock_fd);
}

void handle_handshake_init(const char *rbuf, struct sockaddr_in *saddr, socklen_t saddr_len) {
    static char buf[MAX_MESSAGE_SIZE];
    static char bufaddr[128];
    straddr_p(saddr, bufaddr);

    uint8 stream_id;
    Stream **stream_ptr;
    do {
        stream_id = next_stream_id++;
        stream_ptr = &streams[stream_id];
    } while (*stream_ptr != NULL);
    printf("[%s] Stream ID: %d\n", bufaddr, stream_id);

    Stream *stream = (Stream *)malloc(sizeof(Stream));
    stream->id = stream_id;
    stream->saddr = *saddr;
    *stream_ptr = stream;

    unsigned char *peerkey = (unsigned char *)rbuf;
    printf("[%s] Public key: %s\n", bufaddr, ra_crypto_print_key(peerkey, PUBKEY_SIZE));
    if (ra_crypto_compute_shared_key(peerkey, stream->key, STREAM_KEY_SIZE, SHARED_KEY_MODE_SERVER)) {
        fprintf(stderr, "[%s] Key exchange failed\n", bufaddr);
        return;
    }
    printf("[%s] Shared secret: %s\n", bufaddr, ra_crypto_print_key(stream->key, STREAM_KEY_SIZE));

    char *p = buf;
    p += message_format_type(buf, MESSAGE_TYPE_HANDSHAKE_RESPONSE);
    *(p++) = stream_id;
    p += ra_crypto_write_public_key((unsigned char *)p);
    sendto(sock_fd, buf, p - buf, 0, (struct sockaddr *)saddr, saddr_len);
}

void handle_stream(const char *rbuf, struct sockaddr_in *saddr, socklen_t saddr_len) {
    const char *p = rbuf;
}

static void process_message(const char *buf, int buflen, struct sockaddr_in *saddr, socklen_t saddr_len) {
    MessageType type;
    const char *p = buf;
    p += message_parse_type(p, &type);
    switch (type) {
    case MESSAGE_TYPE_HANDSHAKE_INIT:
        handle_handshake_init(p, saddr, saddr_len);
        break;
    case MESSAGE_TYPE_STREAM:
        handle_stream(p, saddr, saddr_len);
        break;
    case MESSAGE_TYPE_TERMINATE:
        break;
    default:
        break;
    }
}

int main() {
    int err;
    int rc = 0;
    unsigned char pubkey[PUBKEY_SIZE];

    if (ra_crypto_init() < 0) {
        fprintf(stderr, "Failed to initialize cryptographic keys\n");
        goto err_cleanup;
    }
    ra_crypto_write_public_key(pubkey);
    printf("Public key: %s\n", ra_crypto_print_key(pubkey, PUBKEY_SIZE));

    sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock_fd <= 0) {
        perror("socket");
        goto err_cleanup;
    }

    int opt = 1;
    err = setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int));
    if (err) {
        perror("setsockopt");
        goto err_cleanup;
    }

    struct sockaddr_in saddr = {0};
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = INADDR_ANY;
    saddr.sin_port = htons(LISTENER_PORT);
    socklen_t saddr_len = sizeof(struct sockaddr_in);

    err = bind(sock_fd, (struct sockaddr *)&saddr, saddr_len);
    if (err) {
        perror("bind");
        goto err_cleanup;
    }
    printf("Sink listening at port %d\n", LISTENER_PORT);

    is_running = true;
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    char buf[MAX_MESSAGE_SIZE];
    while (is_running) {
        int n = recvfrom(sock_fd, buf, MAX_MESSAGE_SIZE, 0, (struct sockaddr *)&saddr, &saddr_len);
        if (n <= 0) {
            break;
        }
        process_message(buf, n, &saddr, saddr_len);
    }
    goto cleanup;

err_cleanup:
    rc = 1;
cleanup:
    if (sock_fd > 0) close(sock_fd);
    printf("Sink stopped gracefully.\n");
    return rc;
}
