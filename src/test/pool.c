#include "lib/pool.h"

#include <criterion/criterion.h>
#include <stdio.h>

#define POOL_BUFFER_SIZE 64

Test(pool, main) {
    ra_pool_t pool;
    ra_pool_init(&pool, POOL_BUFFER_SIZE, 10);
    char *buf = (char *)ra_pool_acquire(&pool);
    snprintf(buf, POOL_BUFFER_SIZE, "Hello, world!");
    ra_pool_release(&pool, buf);
    ra_pool_deinit(&pool);
}
