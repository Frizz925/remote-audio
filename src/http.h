#include <unistd.h>

#define HTTP_STATUS_OK                    200 
#define HTTP_STATUS_BAD_REQUEST           400 
#define HTTP_STATUS_UNAUTHORIZED          401 
#define HTTP_STATUS_FORBIDDEN             403 
#define HTTP_STATUS_NOT_FOUND             404 
#define HTTP_STATUS_METHOD_NOT_ALLOWED    405 
#define HTTP_STATUS_VERSION_NOT_SUPPORTED 505 

#define HTTP_REASON_200 "OK"
#define HTTP_REASON_400 "Bad Request"
#define HTTP_REASON_401 "Unauthorized"
#define HTTP_REASON_403 "Forbidden"
#define HTTP_REASON_404 "Not Found"
#define HTTP_REASON_405 "Method Not Allowed"
#define HTTP_REASON_505 "HTTP Version Not Supported"

#define HTTP_VERSION_1_1 "HTTP/1.1"

#define HTTP_MAX_HEADERS 64

typedef unsigned int http_status;
typedef unsigned int httplen_t;

struct http_header {
    const char *name;
    const char *value;
};

struct http_headers {
    struct http_header hdrs[HTTP_MAX_HEADERS];
    httplen_t hdrlen;
};

struct http_request_header {
    const char *method, *path, *version;
};

struct http_response_header {
    const char *version, *reason;
    http_status code;
    struct http_headers hdrs;
};

void http_read_request_header(char **buf, struct http_request_header *hdr);

int http_write_response_header(char *buf, const struct http_response_header *hdr);

int http_write_response_code(char *buf, http_status code);

void http_set_header(struct http_headers *hdrs, const char *name, const char *value);

const char *http_get_status_reason(http_status code);