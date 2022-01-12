#include "logger.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "lib/logger.h"
#include "string.h"

#define LOG_BUFSIZE       512
#define LOG_TIMESTAMP_LEN 20
#define LOG_LEVEL_LEN     6

#define LOG_LEVEL_DEBUG 0
#define LOG_LEVEL_INFO  1
#define LOG_LEVEL_WARN  2
#define LOG_LEVEL_ERROR 3
#define LOG_LEVEL_FATAL 4

#define logger_fprintf(logger, level, fmt)       \
    {                                            \
        va_list ap;                              \
        va_start(ap, fmt);                       \
        logger_vfprintf(logger, level, fmt, ap); \
        va_end(ap);                              \
    }

static const char *log_level_str(int level) {
    switch (level) {
    case LOG_LEVEL_DEBUG:
        return "DEBUG";
    case LOG_LEVEL_INFO:
        return "INFO";
    case LOG_LEVEL_WARN:
        return "WARN";
    case LOG_LEVEL_ERROR:
        return "ERROR";
    case LOG_LEVEL_FATAL:
        return "FATAL";
    default:
        return NULL;
    }
}

static void logger_vfprintf(ra_logger_t *logger, int level, const char *fmt, va_list varg) {
    ra_logger_stream_t *stream = logger->stream;
    FILE *f = level <= LOG_LEVEL_INFO ? stream->out : stream->out;
    if (logger->context != NULL) fprintf(f, "[%s] ", logger->context);

    char timestamp[LOG_TIMESTAMP_LEN];
    time_t now = time(NULL);
    strftime(timestamp, LOG_TIMESTAMP_LEN, "%Y-%m-%d %H:%M:%S", localtime(&now));
    fprintf(f, "%s %-5s ", timestamp, log_level_str(level));

    vfprintf(f, fmt, varg);
    fputc('\n', f);
    fflush(f);
}

ra_logger_stream_t *ra_logger_stream_create(FILE *out, FILE *err) {
    ra_logger_stream_t *stream = (ra_logger_stream_t *)malloc(sizeof(ra_logger_stream_t));
    stream->out = out;
    stream->err = err;
    return stream;
}

ra_logger_stream_t *ra_logger_stream_create_default() {
    return ra_logger_stream_create(stdout, stderr);
}

void ra_logger_stream_destroy(ra_logger_stream_t *stream) {
    if (stream->out != stdout) fclose(stream->out);
    if (stream->out != stderr) fclose(stream->err);
    free(stream);
}

ra_logger_t *ra_logger_create(ra_logger_stream_t *stream, const char *context) {
    ra_logger_t *logger = (ra_logger_t *)malloc(sizeof(ra_logger_t));
    logger->stream = stream;
    logger->context = context;
    return logger;
}

void ra_logger_destroy(ra_logger_t *logger) {
    free(logger);
}

void ra_logger_debug(ra_logger_t *logger, const char *fmt, ...) {
    logger_fprintf(logger, LOG_LEVEL_DEBUG, fmt);
}

void ra_logger_info(ra_logger_t *logger, const char *fmt, ...) {
    logger_fprintf(logger, LOG_LEVEL_INFO, fmt);
}

void ra_logger_warn(ra_logger_t *logger, const char *fmt, ...) {
    logger_fprintf(logger, LOG_LEVEL_WARN, fmt);
}

void ra_logger_error(ra_logger_t *logger, const char *fmt, ...) {
    logger_fprintf(logger, LOG_LEVEL_ERROR, fmt);
}

void ra_logger_fatal(ra_logger_t *logger, const char *fmt, ...) {
    logger_fprintf(logger, LOG_LEVEL_FATAL, fmt);
    exit(EXIT_FAILURE);
}
