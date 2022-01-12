#ifndef _RA_LOGGER_H
#define _RA_LOGGER_H

#include <stdio.h>

typedef struct {
    FILE *out;
    FILE *err;
} ra_logger_stream_t;

typedef struct {
    ra_logger_stream_t *stream;
    const char *context;
} ra_logger_t;

ra_logger_stream_t *ra_logger_stream_create(FILE *out, FILE *err);
ra_logger_stream_t *ra_logger_stream_create_default();
void ra_logger_stream_destroy(ra_logger_stream_t *stream);

ra_logger_t *ra_logger_create(ra_logger_stream_t *stream, const char *context);
void ra_logger_destroy(ra_logger_t *logger);

void ra_logger_debug(ra_logger_t *logger, const char *fmt, ...);
void ra_logger_info(ra_logger_t *logger, const char *fmt, ...);
void ra_logger_warn(ra_logger_t *logger, const char *fmt, ...);
void ra_logger_error(ra_logger_t *logger, const char *fmt, ...);
void ra_logger_fatal(ra_logger_t *logger, const char *fmt, ...);
#endif
