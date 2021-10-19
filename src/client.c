#include "client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "http.h"

static int client_process_request(char *rbuf, char *wbuf, int *size) {
    struct http_request_header req_hdr;
    http_read_request_header(&rbuf, &req_hdr);
    if (strcasecmp(req_hdr.method, "GET") != 0) {
        *size = http_write_response_code(wbuf, HTTP_STATUS_METHOD_NOT_ALLOWED);
        return EXIT_FAILURE;
    }
    int major, minor;
    sscanf(req_hdr.version, "HTTP/%d.%d", &major, &minor);
    if (major != 1 || minor > 1) {
        *size = http_write_response_code(wbuf, HTTP_STATUS_VERSION_NOT_SUPPORTED);
        return EXIT_FAILURE;
    }
    *size = http_write_response_code(wbuf, HTTP_STATUS_OK);
    return EXIT_SUCCESS;
}

static int client_process(struct connection *client) {
    int n;
    char rbuf[BUFFER_SIZE], wbuf[BUFFER_SIZE];

    n = read(client->fd, rbuf, BUFFER_SIZE);
    if (n <= 0) {
        perror("read");
        return EXIT_FAILURE;
    }
    rbuf[n] = '\0';

    int rc = client_process_request(rbuf, wbuf, &n);
    send(client->fd, wbuf, n, 0);
    return rc;
}

int client_handle(struct connection *client) {
    char addr[ADDRPORT_STRLEN];
    straddr(addr, &client->addr_in);

    int rc = client_process(client);
    shutdown(client->fd, SHUT_RDWR);
    close(client->fd);
    printf("Closed connection from %s\n", addr);
    return rc;
}