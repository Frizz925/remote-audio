#ifndef _H_STREAM
#define _H_STREAM

#include <arpa/inet.h>
#include <sodium.h>

#include "types.h"

#define STREAM_KEY_SIZE 32

typedef struct {
    uint8 id;
    unsigned char key[STREAM_KEY_SIZE];
    struct sockaddr_in saddr;
} Stream;
#endif
