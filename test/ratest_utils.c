#include <assert.h>
#include <string.h>

#include "../src/utils.h"

int main() {
    char example[32];
    size_t sz_example = sizeof(example);

    size_t sz_buf = 32;
    ra_ringbuf_t *rb = ra_ringbuf_create(sz_buf);

    assert(ra_ringbuf_size(rb) == sz_buf);
    assert(ra_ringbuf_fill_count(rb) == 0);
    assert(ra_ringbuf_free_count(rb) == sz_buf);

    char *wptr = ra_ringbuf_write_ptr(rb);
    const char *rptr = ra_ringbuf_read_ptr(rb);
    assert(wptr == rptr);

    memcpy(wptr, example, sz_example);
    ra_ringbuf_advance_write_ptr(rb, sz_example);
    assert(ra_ringbuf_free_count(rb) == (sz_buf - sz_example));
    assert(ra_ringbuf_fill_count(rb) == sz_example);

    assert(memcmp(rptr, example, sz_example) == 0);
    ra_ringbuf_advance_read_ptr(rb, sz_example);
    assert(ra_ringbuf_fill_count(rb) == 0);

    ra_ringbuf_reset(rb);
    assert(ra_ringbuf_fill_count(rb) == 0);
    assert(ra_ringbuf_free_count(rb) == sz_buf);

    ra_ringbuf_destroy(rb);
    return 0;
}
