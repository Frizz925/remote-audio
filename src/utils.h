#ifndef _RA_UTILS_H
#define _RA_UTILS_H

#include <stdatomic.h>
#include <stdint.h>

typedef struct ra_ringbuf_t ra_ringbuf_t;

ra_ringbuf_t *ra_ringbuf_create(size_t size);
size_t ra_ringbuf_size(ra_ringbuf_t *rb);
size_t ra_ringbuf_fill_count(ra_ringbuf_t *rb);
size_t ra_ringbuf_free_count(ra_ringbuf_t *rb);
const char *ra_ringbuf_read_ptr(ra_ringbuf_t *rb);
char *ra_ringbuf_write_ptr(ra_ringbuf_t *rb);
void ra_ringbuf_advance_read_ptr(ra_ringbuf_t *rb, size_t count);
void ra_ringbuf_advance_write_ptr(ra_ringbuf_t *rb, size_t count);
void ra_ringbuf_reset(ra_ringbuf_t *rb);
void ra_ringbuf_destroy(ra_ringbuf_t *rb);

#endif
