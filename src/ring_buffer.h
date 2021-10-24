#ifndef _RA_RINGBUFFER_H
#define _RA_RINGBUFFER_H

#include <stdlib.h>

struct RingBuffer;
typedef struct RingBuffer RingBuffer;

RingBuffer *ring_buffer_create(size_t capacity);
const void *ring_buffer_reader(RingBuffer *rb, size_t *count);
void *ring_buffer_writer(RingBuffer *rb, size_t *count);
void ring_buffer_advance_reader(RingBuffer *rb, size_t count);
void ring_buffer_advance_writer(RingBuffer *rb, size_t count);
void ring_buffer_destroy(RingBuffer *rb);
#endif
