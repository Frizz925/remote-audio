#include "config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "string.h"

#define DEFAULT_SECTION "default"

struct ra_config_entry_t;
typedef struct ra_config_entry_t ra_config_entry_t;

struct ra_config_entry_t {
    char *key;
    char *value;
    ra_config_entry_t *next;
};

struct ra_config_section_t {
    char *name;
    ra_config_entry_t *entry_head;
    ra_config_entry_t *entry_tail;
    ra_config_section_t *next;
};

struct ra_config_t {
    ra_config_section_t *section_head;
    ra_config_section_t *section_tail;
};

static char *string_unescape(char *str) {
    char buf[65535];
    char *wptr = buf;
    for (char *rptr = str; *rptr; rptr++) {
        if (*rptr == '\\') continue;
        *wptr++ = *rptr;
    }
    *wptr++ = '\0';
    return strcpy(str, buf);
}

static char *string_create(const char *buf, size_t len) {
    char *str = (char *)calloc(len + 1, 1);
    strncpy(str, buf, len);
    strstrip(str);
    return str;
}

static ra_config_section_t *section_create(const char *buf, size_t len) {
    ra_config_section_t *section = (ra_config_section_t *)malloc(sizeof(ra_config_section_t));
    section->name = string_create(buf, len);
    section->entry_head = NULL;
    section->entry_tail = NULL;
    section->next = NULL;
    return section;
}

static ra_config_entry_t *entry_create(const char *buf, size_t len) {
    ra_config_entry_t *entry = (ra_config_entry_t *)malloc(sizeof(ra_config_entry_t));
    entry->key = string_create(buf, len);
    entry->value = NULL;
    entry->next = NULL;
    return entry;
}

static char *value_create(const char *buf, size_t len) {
    char *str = string_create(buf, len);
    string_unescape(str);
    return str;
}

static int config_parse(ra_config_t *cfg, const char *buf, size_t len, int flag) {
    ra_config_section_t *prev_section = cfg->section_tail;
    ra_config_entry_t *prev_entry = prev_section ? prev_section->entry_tail : NULL;

    const char *p = buf, *q;
    for (; p - buf < len; p++) {
        char c = *p;
        switch (flag) {
        case 0:  // Initial state
            if (c == '[') {
                q = ++p;
                flag = 1;
            } else if (c != ' ' && c != '\r' && c != '\n') {
                q = p;
                flag = 2;
            }
            break;
        case 1:  // Section reading
            if (c != ']') break;

            // Create section
            ra_config_section_t *section = section_create(q, p - q);
            if (!prev_section)
                cfg->section_head = section;
            else
                prev_section->next = section;
            cfg->section_tail = prev_section = section;
            flag = 0;
            break;
        case 2:  // Key reading
            if (c != '=') break;

            // Create value
            ra_config_entry_t *entry = entry_create(q, p - q);
            if (!prev_section) {
                const char *name = DEFAULT_SECTION;
                prev_section = section_create(name, strlen(name));
                cfg->section_head = prev_section;
            }
            if (!prev_entry)
                prev_section->entry_head = entry;
            else
                prev_entry->next = entry;
            prev_section->entry_tail = prev_entry = entry;
            flag = 3;
            break;
        case 3:  // Value reading, start after whitespace
            if (c == ' ' || c == '\r' && c == '\n') break;
            if (c == '"') {
                q = p + 1;
                flag = 5;
            } else {
                q = p;
                flag = 4;
            }
            break;
        case 4:  // Value reading, end before EOL
            if (c != '\r' && c != '\n') break;
            if (prev_entry) prev_entry->value = value_create(q, p - q);
            flag = 0;
            break;
        case 5:  // Value reading, end before quotemark
            if (c != '\"') break;
            if (prev_entry) prev_entry->value = value_create(q, p - q);
            flag = 0;
            break;
        }
    }
    return flag;
}

ra_config_t *ra_config_create() {
    ra_config_t *cfg = (ra_config_t *)malloc(sizeof(ra_config_t));
    cfg->section_head = NULL;
    cfg->section_tail = NULL;
    return cfg;
}

ssize_t ra_config_open(ra_config_t *cfg, const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) return -1;
    size_t read = ra_config_read(cfg, f);
    fclose(f);
    return read;
}

size_t ra_config_read(ra_config_t *cfg, FILE *f) {
    char buf[65535];
    int flag = 0;
    size_t read = 0;
    while (!feof(f)) {
        size_t len = fread(buf, 1, sizeof(buf), f);
        if (len <= 0) break;
        flag = config_parse(cfg, buf, len, flag);
        read += len;
    }
    return read;
}

void ra_config_parse(ra_config_t *cfg, const char *buf, size_t len) {
    config_parse(cfg, buf, len, 0);
}

ra_config_section_t *ra_config_get_section(ra_config_t *cfg, const char *name) {
    if (!cfg) return NULL;
    ra_config_section_t *section = cfg->section_head;
    while (section) {
        if (strequal(section->name, name)) return section;
        section = section->next;
    }
    return NULL;
}

ra_config_section_t *ra_config_get_default_section(ra_config_t *cfg) {
    return ra_config_get_section(cfg, DEFAULT_SECTION);
}

const char *ra_config_get_value(ra_config_section_t *section, const char *key) {
    if (!section) return NULL;
    ra_config_entry_t *entry = section->entry_head;
    while (entry) {
        if (strequal(entry->key, key)) return entry->value;
        entry = entry->next;
    }
    return NULL;
}

void ra_config_destroy(ra_config_t *cfg) {
    if (!cfg) return;
    ra_config_section_t *section = cfg->section_head;
    while (section != NULL) {
        ra_config_entry_t *entry = section->entry_head;
        while (entry != NULL) {
            ra_config_entry_t *next = entry->next;
            free(entry->key);
            free(entry->value);
            free(entry);
            entry = next;
        }
        ra_config_section_t *next = section->next;
        free(section->name);
        free(section);
        section = next;
    }
    free(cfg);
}
