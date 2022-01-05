#include "config.h"

#include <stdlib.h>

ra_config_t *ra_config_open(const char *filename) {
    return NULL;
}

ra_config_section_t *ra_config_get_section(ra_config_t *cfg, const char *name) {
    return NULL;
}

ra_config_section_t *ra_config_get_default_section(ra_config_t *cfg) {
    return NULL;
}

const char *ra_config_get_value(ra_config_section_t *section, const char *name) {
    return NULL;
}

void ra_config_destroy(ra_config_t *cfg) {}
