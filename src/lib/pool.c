#include "pool.h"

#include <stdlib.h>

struct ra_pool_item_s {
    ra_pool_item_t *next;
};

void ra_pool_init(ra_pool_t *pool, size_t size, size_t capacity) {
    pool->head = pool->tail = NULL;
    pool->count = 0;
    pool->size = size;
    pool->capacity = capacity;
}

size_t ra_pool_size(ra_pool_t *pool) {
    return pool->size;
}

void *ra_pool_acquire(ra_pool_t *pool) {
    void *ptr = pool->count > 0 ? pool->head : NULL;
    if (ptr != NULL) {
        ra_pool_item_t *item = (ra_pool_item_t *)ptr;
        pool->count--;
        pool->head = item->next;
        if (pool->head == pool->tail)
            pool->head = pool->tail = NULL;
        else
            pool->head = item->next;
        item->next = NULL;
    } else {
        ptr = malloc(sizeof(ra_pool_item_t) + pool->size);
        ra_pool_item_t *item = (ra_pool_item_t *)ptr;
        item->next = NULL;
    }
    return ptr + sizeof(ra_pool_item_t);
}

void ra_pool_release(ra_pool_t *pool, void *ptr) {
    if (!pool || pool->count >= pool->capacity) {
        free(ptr);
        return;
    }
    ra_pool_item_t *item = (ra_pool_item_t *)(ptr - sizeof(ra_pool_item_t));
    if (pool->tail != NULL) {
        pool->tail->next = item;
        pool->tail = item;
    } else {
        pool->head = pool->tail = item;
    }
    pool->count++;
}

void ra_pool_deinit(ra_pool_t *pool) {
    ra_pool_item_t *cur = pool->head;
    while (cur) {
        ra_pool_item_t *tmp = cur;
        cur = cur->next;
        free(tmp);
    }
    pool->head = pool->tail = NULL;
}
