#include <assert.h>

#include "../src/types.h"

static void test_uint64() {
    uint64_t hostval = 0xDEADBEEF;
    uint64_t netval = htonll(hostval);
    assert(ntohll(netval) == hostval);
}

int main() {
    test_uint64();
    return 0;
}
