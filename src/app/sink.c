#include "sink.h"

#include <stdlib.h>
#include <uv.h>

int sink_main(int argc, char **argv) {
    uv_loop_t loop;
    uv_loop_close(&loop);
    return EXIT_SUCCESS;
}
