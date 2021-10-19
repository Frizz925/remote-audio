#include <arpa/inet.h>

#define LISTENER_PORT    27100
#define LISTENER_BACKLOG 5
#define ADDRPORT_STRLEN  32
#define BUFFER_SIZE      1024

struct connection {
    int fd;
    struct sockaddr_in addr_in;
};

int straddr(char *result, struct sockaddr_in *addr_in);