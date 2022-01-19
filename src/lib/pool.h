#ifndef _RA_POOL_H
#define _RA_POOL_H
#include <stddef.h>

struct ra_pool_item_s;
typedef struct ra_pool_item_s ra_pool_item_t;

typedef struct {
    ra_pool_item_t *head;
    ra_pool_item_t *tail;
    size_t size;
    size_t count;
    size_t capacity;
} ra_pool_t;

void ra_pool_init(ra_pool_t *pool, size_t size, size_t capacity);
void *ra_pool_acquire(ra_pool_t *pool);
void ra_pool_release(ra_pool_t *pool, void *ptr);
void ra_pool_deinit(ra_pool_t *pool);
#endif
