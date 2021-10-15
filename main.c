#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define LISTENER_PORT    27100
#define LISTENER_BACKLOG 5
#define BUFFER_SIZE      1024
#define ADDRPORT_STRLEN 32

static int is_parent = 1;

struct connection {
    int fd;
    struct sockaddr_in addr_in;
};

static int printf_err(const char *format, ...) {
    int result;
    va_list ap;
    va_start(ap, format);
    result = vfprintf(stderr, format, ap);
    va_end(ap);
    return result;
}

static int straddr(char *result, struct sockaddr_in *addr_in) {
    char buf[INET_ADDRSTRLEN];
    const char *addr = inet_ntop(AF_INET, &addr_in->sin_addr, buf, INET_ADDRSTRLEN);
    return sprintf(result, "%s:%d", addr, ntohs(addr_in->sin_port));
}

static int stream(struct connection *client) {
    int n;
    char buf[BUFFER_SIZE];
    char addr[ADDRPORT_STRLEN];
    straddr(addr, &client->addr_in);
    for (;;) {
        n = read(client->fd, buf, BUFFER_SIZE);
        if (n <= 0) break;
        buf[n-1] = '\n';
        buf[n] = '\0';
        printf("From %s: %s", addr, buf);
    }
    return EXIT_SUCCESS;
}

static int serve(struct connection *server, struct connection *client) {
    int rc = 0, opt = 1;
    int addrlen = sizeof(server->addr_in);
    char addr[ADDRPORT_STRLEN];

    server->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->fd < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }
    if (setsockopt(server->fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        return EXIT_FAILURE;
    }

    server->addr_in.sin_family = AF_INET;
    server->addr_in.sin_addr.s_addr = INADDR_ANY;
    server->addr_in.sin_port = htons(LISTENER_PORT);
    straddr(addr, &server->addr_in);
    if (bind(server->fd, (struct sockaddr *)&server->addr_in, addrlen)) {
        perror("bind");
        return EXIT_FAILURE;
    }
    if (listen(server->fd, LISTENER_BACKLOG)) {
        perror("listen");
        return EXIT_FAILURE;
    }
    printf("Server listening at %s\n", addr);

    int n;
    while (is_parent) {
        addrlen = sizeof(client->addr_in);
        client->fd = accept(server->fd, (struct sockaddr *)&client->addr_in, (socklen_t *)&addrlen);
        if (client->fd <= 0) {
            perror("accept");
            return EXIT_FAILURE;
        }
        straddr(addr, &client->addr_in);
        printf("Accepted connection from %s\n", addr);
        is_parent = fork() != 0;
    }

    return EXIT_SUCCESS;
}

int main() {
    char addr[ADDRPORT_STRLEN];
    struct connection server, client;
    memset(&server, 0, sizeof(struct connection));
    memset(&client, 0, sizeof(struct connection));

    int rc = serve(&server, &client);
    if (is_parent && server.fd) {
        close(server.fd);
        straddr(addr, &server.addr_in);
        printf("Closed listener at %s\n", addr);
    } else if (client.fd) {
        if (!rc) rc = stream(&client);
        close(client.fd);
        straddr(addr, &client.addr_in);
        printf("Closed connection from %s\n", addr);
    }
    return rc;
}