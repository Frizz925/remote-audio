#ifndef _RA_TYPES_H
#define _RA_TYPES_H
#include <stdint.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <WinSock2.h>
uint64_t htonll(uint64_t hostval);
uint64_t ntohll(uint64_t netval);
#else
#include <arpa/inet.h>
#endif

typedef union {
    uint16_t value;
    char buf[2];
} uint16_raw_t;

typedef union {
    uint32_t value;
    char buf[4];
} uint32_raw_t;

typedef union {
    uint64_t value;
    char buf[8];
} uint64_raw_t;

void uint16_to_bytes(char *buf, uint16_t value);
void uint32_to_bytes(char *buf, uint32_t value);
void uint64_to_bytes(char *buf, uint64_t value);

uint16_t bytes_to_uint16(const char *buf);
uint32_t bytes_to_uint32(const char *buf);
uint64_t bytes_to_uint64(const char *buf);
#endif
