#ifndef _RA_LOGGER_H
#define _RA_LOGGER_H
#include <uv.h>

#include "pool.h"

typedef struct {
    uv_stream_t *out;
    uv_stream_t *err;
} ra_logger_stream_t;

typedef struct {
    ra_pool_t pool;
    ra_logger_stream_t *stream;
    const char *context;
} ra_logger_t;

void ra_logger_stream_init(ra_logger_stream_t *stream, uv_stream_t *out, uv_stream_t *err);

void ra_logger_init(ra_logger_t *logger, ra_logger_stream_t *stream, const char *context);
void ra_logger_deinit(ra_logger_t *logger);

void ra_logger_trace(ra_logger_t *logger, const char *fmt, ...);
void ra_logger_debug(ra_logger_t *logger, const char *fmt, ...);
void ra_logger_info(ra_logger_t *logger, const char *fmt, ...);
void ra_logger_warn(ra_logger_t *logger, const char *fmt, ...);
void ra_logger_error(ra_logger_t *logger, const char *fmt, ...);
void ra_logger_fatal(ra_logger_t *logger, const char *fmt, ...);
#endif
