#include "../src/ring_buffer.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int ring_buffer_test() {
    const char *expected = "Hello, world!";
    size_t expectedlen = strlen(expected);
    size_t capacity = 32;
    RingBuffer *rb = ring_buffer_create(capacity);
    char actual[32];

    size_t free_count;
    char *wptr = (char *)ring_buffer_writer(rb, &free_count);
    assert(free_count == capacity);
    int write_count = sprintf(wptr, expected);
    assert(write_count == expectedlen);
    ring_buffer_advance_writer(rb, write_count);

    size_t fill_count;
    const char *rptr = (const char *)ring_buffer_reader(rb, &fill_count);
    assert(fill_count == write_count);
    strcpy(actual, rptr);
    int read_count = strlen(actual);
    assert(read_count == expectedlen);
    assert(!strcmp(expected, actual));
    ring_buffer_advance_reader(rb, read_count);

    ring_buffer_destroy(rb);
    return EXIT_SUCCESS;
}
