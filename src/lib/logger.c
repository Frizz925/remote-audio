#include "logger.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <uv.h>

#include "pool.h"

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

struct logger_req_s;
typedef struct logger_req_s logger_req_t;

struct logger_req_s {
    uv_fs_t base;
    uv_buf_t buf;
    size_t capbuf;
};

struct ra_logger_s {
    const char *context;
    ra_logger_stream_t *stream;
    ra_pool_t *pool;
};

static const char *log_level_str[] = {
    "TRACE",
    "DEBUG",
    "INFO",
    "WARN",
    "ERROR",
    "FATAL",
};

static logger_req_t *logger_req_acquire(ra_pool_t *pool) {
    void *ptr = ra_pool_acquire(pool);
    logger_req_t *req = (logger_req_t *)ptr;
    req->base.data = pool;
    req->buf.base = (char *)ptr + sizeof(logger_req_t);
    req->buf.len = 0;
    req->capbuf = LOGGER_BUFFER_SIZE;
    return req;
}

static void logger_write_cb(uv_fs_t *req) {
    ra_pool_release((ra_pool_t *)req->data, req);
}

static void logger_vprintf(ra_logger_t *logger, int level, const char *fmt, va_list varg) {
    ra_logger_stream_t *stream = logger->stream;
    logger_req_t *req = logger_req_acquire(logger->pool);
    uv_buf_t *buf = &req->buf;
    char *base = buf->base;
    char *wptr = base;
    char *endptr = wptr + req->capbuf;

    // Context
    if (logger->context != NULL) wptr += snprintf(wptr, endptr - wptr, "[%s] ", logger->context);
    // Timestamp
    time_t now = time(NULL);
    wptr += strftime(wptr, endptr - wptr, LOG_TIMESTAMP_FORMAT " ", localtime(&now));
    // Log level
    const char *s_level = level >= LOG_LEVEL_TRACE && level <= LOG_LEVEL_FATAL ? log_level_str[level] : NULL;
    wptr += snprintf(wptr, endptr - wptr, "%-5s ", s_level);
    // Message
    wptr += vsnprintf(wptr, endptr - wptr, fmt, varg);
    wptr += snprintf(wptr, endptr - wptr, "\n");

    buf->len = wptr - base;
    uv_fs_write(stream->loop,
                (uv_fs_t *)req,
                level <= LOG_LEVEL_INFO ? stream->out : stream->err,
                buf,
                1,
                0,
                logger_write_cb);
}

ra_logger_stream_t *ra_logger_stream_new(uv_loop_t *loop, uv_file out, uv_file err) {
    ra_logger_stream_t *stream = (ra_logger_stream_t *)malloc(sizeof(ra_logger_stream_t));
    stream->loop = loop;
    stream->out = out;
    stream->err = err;
    return stream;
}

ra_logger_stream_t *ra_logger_stream_default(uv_loop_t *loop) {
    return ra_logger_stream_new(loop, STDOUT_FILENO, STDERR_FILENO);
}

void ra_logger_stream_free(ra_logger_stream_t *stream) {
    free(stream);
}

ra_logger_t *ra_logger_new(ra_logger_stream_t *stream, const char *context) {
    ra_logger_t *logger = (ra_logger_t *)calloc(1, sizeof(ra_logger_t));
    logger->context = context;
    logger->stream = stream;
    logger->pool = ra_pool_new(sizeof(logger_req_t) + LOGGER_BUFFER_SIZE, LOGGER_POOL_CAPACITY);
    return logger;
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

void ra_logger_free(ra_logger_t *logger) {
    ra_pool_free(logger->pool);
    free(logger);
}
