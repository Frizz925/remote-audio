#include <assert.h>

#include "lib/config.h"
#include "lib/string.h"

const char *configstr =
    "[sink]\n"
    "port = 21500\n"
    "device = \"CABLE Input\"\n"
    "\n"
    "[source]\n"
    "host = 127.0.0.1\n"
    "port = 21500\n"
    "device = \"VB-Cable\"\n";

int main(int argc, char **argv) {
    ra_config_t *cfg = ra_config_create();
    ra_config_parse(cfg, configstr, strlen(configstr));

    // No default section
    assert(!ra_config_get_default_section(cfg));

    // Sink section
    ra_config_section_t *sink = ra_config_get_section(cfg, "sink");
    assert(sink);
    assert(strequal(ra_config_get_value(sink, "port"), "21500"));
    assert(strequal(ra_config_get_value(sink, "device"), "CABLE Input"));
    assert(!ra_config_get_value(sink, "invalid"));

    // Sink section
    ra_config_section_t *source = ra_config_get_section(cfg, "source");
    assert(source);
    assert(strequal(ra_config_get_value(source, "host"), "127.0.0.1"));
    assert(strequal(ra_config_get_value(source, "port"), "21500"));
    assert(strequal(ra_config_get_value(source, "device"), "VB-Cable"));
    assert(!ra_config_get_value(source, "invalid"));

    ra_config_destroy(cfg);
    return 0;
}
