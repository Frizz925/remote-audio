#include "utils.h"

#include <stdbool.h>
#include <stdlib.h>

#define RINGBUF_STATE_EMPTY 0
#define RINGBUF_STATE_FILLED 1
#define RINGBUF_STATE_FULL 2

struct ra_ringbuf_t {
    char *buf;
    size_t size;
    atomic_ulong read_idx;
    atomic_ulong write_idx;
    atomic_uchar state;
};

ra_ringbuf_t *ra_ringbuf_create(size_t size) {
    ra_ringbuf_t *rb = calloc(1, sizeof(ra_ringbuf_t));
    rb->buf = malloc(size);
    rb->size = size;
    return rb;
}

size_t ra_ringbuf_size(ra_ringbuf_t *rb) {
    return rb->size;
}

size_t ra_ringbuf_fill_count(ra_ringbuf_t *rb) {
    return rb->state != RINGBUF_STATE_EMPTY ? (rb->write_idx > rb->read_idx ? rb->write_idx : rb->size) - rb->read_idx
                                            : 0;
}

size_t ra_ringbuf_free_count(ra_ringbuf_t *rb) {
    return rb->state != RINGBUF_STATE_FULL ? (rb->read_idx > rb->write_idx ? rb->read_idx : rb->size) - rb->write_idx
                                           : 0;
}

const char *ra_ringbuf_read_ptr(ra_ringbuf_t *rb) {
    return rb->buf + rb->read_idx;
}

char *ra_ringbuf_write_ptr(ra_ringbuf_t *rb) {
    return rb->buf + rb->write_idx;
}

void ra_ringbuf_advance_read_ptr(ra_ringbuf_t *rb, size_t count) {
    rb->read_idx = (rb->read_idx + count) % rb->size;
    rb->state = rb->read_idx == rb->write_idx ? RINGBUF_STATE_EMPTY : RINGBUF_STATE_FILLED;
}

void ra_ringbuf_advance_write_ptr(ra_ringbuf_t *rb, size_t count) {
    rb->write_idx = (rb->write_idx + count) % rb->size;
    rb->state = rb->write_idx == rb->read_idx ? RINGBUF_STATE_FULL : RINGBUF_STATE_FILLED;
}

void ra_ringbuf_reset(ra_ringbuf_t *rb) {
    rb->read_idx = 0;
    rb->write_idx = 0;
    rb->state = RINGBUF_STATE_EMPTY;
}

void ra_ringbuf_destroy(ra_ringbuf_t *rb) {
    free(rb->buf);
    free(rb);
}

size_t ra_min(size_t a, size_t b) {
    return a < b ? a : b;
}
