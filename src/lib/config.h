#ifndef _RA_CONFIG_H
#define _RA_CONFIG_H

#include <stdio.h>
#include <stdlib.h>

struct ra_config_t;
typedef struct ra_config_t ra_config_t;

struct ra_config_section_t;
typedef struct ra_config_section_t ra_config_section_t;

ra_config_t *ra_config_create();
ssize_t ra_config_open(ra_config_t *cfg, const char *filename);
size_t ra_config_read(ra_config_t *cfg, FILE *file);
void ra_config_parse(ra_config_t *cfg, const char *buf, size_t len);
ra_config_section_t *ra_config_get_section(ra_config_t *cfg, const char *name);
ra_config_section_t *ra_config_get_default_section(ra_config_t *cfg);
const char *ra_config_get_value(ra_config_section_t *section, const char *key);
void ra_config_destroy(ra_config_t *cfg);
#endif
