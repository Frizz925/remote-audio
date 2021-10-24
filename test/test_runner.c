#include <libgen.h>
#include <stdlib.h>

#include "ring_buffer_test.c"

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <test-case>\n", basename(argv[0]));
        return EXIT_FAILURE;
    }
    const char *test_name = argv[1];
    if (!strcmp(test_name, "ring_buffer_test")) {
        return ring_buffer_test();
    }
    fprintf(stderr, "Unknown test case: %s\n", test_name);
    return EXIT_FAILURE;
}
