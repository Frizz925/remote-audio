#include "ring_buffer.h"

#include <assert.h>
#include <stdatomic.h>
#include <stdlib.h>

struct RingBuffer {
    char *buf;
    atomic_ulong roff, woff;
    size_t capacity;
};

RingBuffer *ring_buffer_create(size_t capacity) {
    RingBuffer *rb = (RingBuffer *)calloc(1, sizeof(RingBuffer));
    rb->buf = (char *)malloc(2 * capacity);
    return rb;
}

const void *ring_buffer_reader(RingBuffer *rb, size_t *count) {
    *count = rb->woff - rb->roff;
    assert(*count >= 0);
    assert(*count <= rb->capacity);
    return rb->buf + (rb->roff % rb->capacity);
}

void *ring_buffer_writer(RingBuffer *rb, size_t *count) {
    *count = rb->capacity - (rb->woff - rb->roff);
    assert(*count >= 0);
    assert(*count <= rb->capacity);
    return rb->buf + (rb->woff % rb->capacity);
}

void ring_buffer_advance_reader(RingBuffer *rb, size_t count) {
    rb->roff += count;
}

void ring_buffer_advance_writer(RingBuffer *rb, size_t count) {
    rb->woff += count;
}

void ring_buffer_destroy(RingBuffer *rb) {
    if (!rb) return;
    if (rb->buf) free(rb->buf);
    free(rb);
}
