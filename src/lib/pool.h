#include <stddef.h>

struct ra_pool_item_s;
typedef struct ra_pool_item_s ra_pool_item_t;

struct ra_pool_s;
typedef struct ra_pool_s ra_pool_t;

ra_pool_t *ra_pool_new(size_t size, size_t capacity);
void *ra_pool_acquire(ra_pool_t *pool);
void ra_pool_release(ra_pool_t *pool, void *ptr);
void ra_pool_free(ra_pool_t *pool);
