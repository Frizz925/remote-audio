#include <uv.h>

typedef struct {
    uv_loop_t *loop;
    uv_file out;
    uv_file err;
} ra_logger_stream_t;

struct ra_logger_s;
typedef struct ra_logger_s ra_logger_t;

ra_logger_stream_t *ra_logger_stream_new(uv_loop_t *loop, uv_file out, uv_file err);
ra_logger_stream_t *ra_logger_stream_default(uv_loop_t *loop);
void ra_logger_stream_free(ra_logger_stream_t *stream);

ra_logger_t *ra_logger_new(ra_logger_stream_t *stream, const char *context);
void ra_logger_trace(ra_logger_t *logger, const char *fmt, ...);
void ra_logger_debug(ra_logger_t *logger, const char *fmt, ...);
void ra_logger_info(ra_logger_t *logger, const char *fmt, ...);
void ra_logger_warn(ra_logger_t *logger, const char *fmt, ...);
void ra_logger_error(ra_logger_t *logger, const char *fmt, ...);
void ra_logger_fatal(ra_logger_t *logger, const char *fmt, ...);
void ra_logger_free(ra_logger_t *logger);
