#include "logger.h"

#include <stdarg.h>
#include <stdlib.h>
#include <time.h>

#include "string.h"

#define LOG_BUFSIZE 512
#define LOG_TIMESTAMP_LEN 20
#define LOG_LEVEL_LEN 6

#define logger_fprintf(f, level, fmt)       \
    {                                       \
        va_list ap;                         \
        va_start(ap, fmt);                  \
        logger_vfprintf(f, level, fmt, ap); \
        va_end(ap);                         \
    }

static void logger_vfprintf(FILE *f, const char *level, const char *fmt, va_list varg) {
    char buffer[LOG_BUFSIZE] = {};
    char *p_end = buffer + LOG_BUFSIZE;

    char *p_ts = buffer;
    time_t now = time(NULL);
    strftime(p_ts, LOG_TIMESTAMP_LEN, "%Y-%m-%d %H:%M:%S", localtime(&now));

    char *p_level = p_ts + LOG_TIMESTAMP_LEN;
    snprintf(p_level, LOG_LEVEL_LEN, "%5s", level);

    char *p_msg = p_level + LOG_LEVEL_LEN + 6;
    vsnprintf(p_msg, p_end - p_msg, fmt, varg);

    fprintf(f, "%s %s %s\n", p_ts, p_level, p_msg);
    fflush(f);
}

ra_logger_t *ra_logger_create(FILE *out, FILE *err) {
    ra_logger_t *logger = (ra_logger_t *)malloc(sizeof(ra_logger_t));
    logger->out = out;
    logger->err = err;
    return logger;
}

ra_logger_t *ra_logger_create_default() {
    return ra_logger_create(stdout, stderr);
}

void ra_logger_destroy(ra_logger_t *logger) {
    if (logger->out != stdout) fclose(logger->out);
    if (logger->out != stderr) fclose(logger->err);
    free(logger);
}

void ra_logger_debug(ra_logger_t *logger, const char *fmt, ...) {
    logger_fprintf(logger->out, "DEBUG", fmt);
}

void ra_logger_info(ra_logger_t *logger, const char *fmt, ...) {
    logger_fprintf(logger->out, "INFO", fmt);
}

void ra_logger_warn(ra_logger_t *logger, const char *fmt, ...) {
    logger_fprintf(logger->err, "WARN", fmt);
}

void ra_logger_error(ra_logger_t *logger, const char *fmt, ...) {
    logger_fprintf(logger->err, "ERROR", fmt);
}

void ra_logger_fatal(ra_logger_t *logger, const char *fmt, ...) {
    logger_fprintf(logger->err, "FATAL", fmt);
    exit(EXIT_FAILURE);
}
