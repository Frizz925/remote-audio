#include <assert.h>

#include "lib/types.h"

static void test_htonll() {
    uint64_raw_t netval = {0};
    netval.value = htonll(1);
    assert(netval.buf[7] == 1);
}

static void test_ntohll() {
    uint64_raw_t netval = {0};
    netval.buf[7] = 1;
    assert(ntohll(netval.value) == 1);
}

int main() {
    test_htonll();
    test_ntohll();
    return 0;
}
