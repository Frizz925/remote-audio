#include "logger.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <uv.h>

#include "lib/logger.h"
#include "lib/pool.h"

#define LOGGER_POOL_CAPACITY 1024
#define LOGGER_BUFFER_SIZE   2048

#define LOG_TIMESTAMP_FORMAT "%Y-%m-%d %H:%M:%S"

#define LOG_LEVEL_TRACE 0
#define LOG_LEVEL_DEBUG 1
#define LOG_LEVEL_INFO  2
#define LOG_LEVEL_WARN  3
#define LOG_LEVEL_ERROR 4
#define LOG_LEVEL_FATAL 5

#define logger_printf(logger, level, fmt)       \
    {                                           \
        va_list ap;                             \
        va_start(ap, fmt);                      \
        logger_vprintf(logger, level, fmt, ap); \
        va_end(ap);                             \
    }

typedef struct {
    uv_write_t base;
    uv_buf_t buf;
    ra_pool_t *pool;
} logger_handle_t;

typedef struct {
    uv_tty_t out;
    uv_tty_t err;
} logger_streams_t;

static const char *log_level_str[] = {
    "TRACE",
    "DEBUG",
    "INFO",
    "WARN",
    "ERROR",
    "FATAL",
};

static logger_handle_t *handle_acquire(ra_pool_t *pool) {
    static size_t sz_handle = sizeof(logger_handle_t);
    void *ptr = ra_pool_acquire(pool);
    logger_handle_t *handle = (logger_handle_t *)ptr;
    handle->buf = uv_buf_init((char *)ptr + sz_handle, pool->size - sz_handle);
    handle->pool = pool;
    return handle;
}

static void logger_write_cb(uv_write_t *req, int status) {
    logger_handle_t *handle = (logger_handle_t *)req;
    ra_pool_release(handle->pool, handle);
}

static void logger_vprintf(ra_logger_t *logger, int level, const char *fmt, va_list varg) {
    ra_pool_t *pool = (ra_pool_t *)logger;
    ra_logger_stream_t *stream = logger->stream;
    uv_stream_t *file = stream->err != NULL && level >= LOG_LEVEL_WARN ? stream->err : stream->out;

    logger_handle_t *handle = handle_acquire(pool);
    uv_buf_t *buf = &handle->buf;
    char *base = buf->base;
    char *wptr = base;
    char *endptr = wptr + buf->len;

    // Timestamp
    time_t now = time(NULL);
    wptr += strftime(wptr, endptr - wptr, LOG_TIMESTAMP_FORMAT " ", localtime(&now));
    // Log level
    const char *s_level = level >= LOG_LEVEL_TRACE && level <= LOG_LEVEL_FATAL ? log_level_str[level] : NULL;
    wptr += snprintf(wptr, endptr - wptr, "%5s ", s_level);
    // Context
    if (logger->context != NULL) wptr += snprintf(wptr, endptr - wptr, "[%s] ", logger->context);
    // Message
    wptr += vsnprintf(wptr, endptr - wptr, fmt, varg);
    wptr += snprintf(wptr, endptr - wptr, "\n");

    buf->len = wptr - base;
    uv_write((uv_write_t *)handle, file, buf, 1, logger_write_cb);
}

void ra_logger_stream_init(ra_logger_stream_t *stream, uv_stream_t *out, uv_stream_t *err) {
    stream->out = out;
    stream->err = err;
}

void ra_logger_init(ra_logger_t *logger, ra_logger_stream_t *stream, const char *context) {
    ra_pool_init((ra_pool_t *)logger, sizeof(logger_handle_t) + LOGGER_BUFFER_SIZE, LOGGER_POOL_CAPACITY);
    logger->stream = stream;
    logger->context = context;
}

void ra_logger_trace(ra_logger_t *logger, const char *fmt, ...) {
    logger_printf(logger, LOG_LEVEL_TRACE, fmt);
    // TODO: Print stacktrace
}

void ra_logger_debug(ra_logger_t *logger, const char *fmt, ...) {
    logger_printf(logger, LOG_LEVEL_DEBUG, fmt);
}

void ra_logger_info(ra_logger_t *logger, const char *fmt, ...) {
    logger_printf(logger, LOG_LEVEL_INFO, fmt);
}

void ra_logger_warn(ra_logger_t *logger, const char *fmt, ...) {
    logger_printf(logger, LOG_LEVEL_WARN, fmt);
}

void ra_logger_error(ra_logger_t *logger, const char *fmt, ...) {
    logger_printf(logger, LOG_LEVEL_ERROR, fmt);
}

void ra_logger_fatal(ra_logger_t *logger, const char *fmt, ...) {
    logger_printf(logger, LOG_LEVEL_FATAL, fmt);
    exit(EXIT_FAILURE);
}

void ra_logger_deinit(ra_logger_t *logger) {
    ra_pool_deinit((ra_pool_t *)logger);
}
