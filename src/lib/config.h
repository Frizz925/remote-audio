#ifndef _RA_CONFIG_H
#define _RA_CONFIG_H

struct ra_config_t;
typedef struct ra_config_t ra_config_t;

struct ra_config_section_t;
typedef struct ra_config_section_t ra_config_section_t;

ra_config_t *ra_config_open(const char *filename);
ra_config_section_t *ra_config_get_section(ra_config_t *cfg, const char *name);
ra_config_section_t *ra_config_get_default_section(ra_config_t *cfg);
const char *ra_config_get_value(ra_config_section_t *section, const char *name);
void ra_config_destroy(ra_config_t *cfg);
#endif
