#include <arpa/inet.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

#include "crypto.h"
#include "message.h"
#include "stream.h"
#include "types.h"
#include "utils.h"

#define LISTENER_HOST "127.0.0.1"
#define LISTENER_PORT 1204
#define BUFFER_SIZE 1200

static int sock_fd = -1;

int main() {
    MessageType msgtype;
    unsigned char pubkey[PUBKEY_SIZE];
    char bufaddr[128];
    int rc = 0;

    if (ra_crypto_init() < 0) {
        fprintf(stderr, "Failed to cryptographic keys\n");
        goto err_cleanup;
    }
    ra_crypto_write_public_key(pubkey);
    printf("Public key: %s\n", ra_crypto_print_key(pubkey, PUBKEY_SIZE));

    sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock_fd <= 0) {
        perror("socket");
        goto err_cleanup;
    }

    struct sockaddr_in saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = inet_addr(LISTENER_HOST);
    saddr.sin_port = htons(LISTENER_PORT);
    socklen_t addrlen = sizeof(struct sockaddr_in);
    straddr_p(&saddr, bufaddr);

    char buf[BUFFER_SIZE];
    char *p = buf;

    p += message_format_type(p, MESSAGE_TYPE_HANDSHAKE_INIT);
    p += ra_crypto_write_public_key((unsigned char *)p);
    if (sendto(sock_fd, buf, p - buf, 0, (struct sockaddr *)&saddr, addrlen) <= 0) {
        perror("sendto");
        goto err_cleanup;
    }

    p = buf;
    if (recvfrom(sock_fd, buf, BUFFER_SIZE, 0, (struct sockaddr *)&saddr, &addrlen) <= 0) {
        perror("recvfrom");
        goto err_cleanup;
    }

    p += message_parse_type(buf, &msgtype);
    if (msgtype != MESSAGE_TYPE_HANDSHAKE_RESPONSE) goto err_cleanup;

    uint8 stream_id = *(p++);
    printf("[%s] Stream ID: %d\n", bufaddr, stream_id);

    unsigned char *peerkey = (unsigned char *)p;
    printf("[%s] Public key: %s\n", bufaddr, ra_crypto_print_key(peerkey, PUBKEY_SIZE));

    unsigned char sharedkey[STREAM_KEY_SIZE];
    if (ra_crypto_compute_shared_key(peerkey, sharedkey, STREAM_KEY_SIZE, SHARED_KEY_MODE_CLIENT)) {
        fprintf(stderr, "[%s] Key exchange failed\n", bufaddr);
        goto err_cleanup;
    }
    printf("[%s] Shared secret: %s\n", bufaddr, ra_crypto_print_key(sharedkey, STREAM_KEY_SIZE));

    p = buf;

    goto cleanup;

err_cleanup:
    rc = 1;
cleanup:
    if (sock_fd > 0) close(sock_fd);
    return rc;
}
