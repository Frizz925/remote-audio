#include "http.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "inet.h"

static int http_write_headers(char *buf, const struct http_headers *hdrs) {
    char *p = buf;
    for (int i = 0; i < hdrs->hdrlen; i++) {
        struct http_header hdr = hdrs->hdrs[i];
        p += sprintf(buf, "%s: %s\r\n", hdr.name, hdr.value);
    }
    return p - buf;
}

void http_read_request_header(char **buf, struct http_request_header *hdr) {
    // Parse method
    hdr->method = strsep(buf, " ");
    hdr->path = strsep(buf, " ");
    hdr->version = strsep(buf, "\r\n");
    *buf += 1;
}

int http_write_response_header(char *buf, const struct http_response_header *hdr) {
    char *p = buf;
    p += sprintf(p, "%s %d %s\r\n", hdr->version, hdr->code, hdr->reason);
    p += http_write_headers(p, &hdr->hdrs);
    p += sprintf(p, "\r\n");
    return p - buf;
}

int http_write_response_code(char *buf, http_status code) {
    struct http_response_header hdr;
    memset(&hdr, 0, sizeof(struct http_response_header));

    hdr.code = code;
    hdr.reason = http_get_status_reason(code);
    hdr.version = HTTP_VERSION_1_1;
    http_set_header(&hdr.hdrs, "Content-Length", "0");

    return http_write_response_header(buf, &hdr);
}

void http_set_header(struct http_headers *hdrs, const char *name, const char *value) {
    int i;
    struct http_header hdr;
    hdr.name = name;
    hdr.value = value;
    for (i = 0; i < hdrs->hdrlen; i++) {
        if (!strcasecmp(hdrs->hdrs[i].name, name)) {
            break;
        }
    }
    if (i < HTTP_MAX_HEADERS) hdrs->hdrs[i] = hdr;
    if (i >= hdrs->hdrlen) hdrs->hdrlen++;
}

const char *http_get_status_reason(http_status code) {
    switch (code) {
        case HTTP_STATUS_OK:
            return HTTP_REASON_200;
        case HTTP_STATUS_BAD_REQUEST:
            return HTTP_REASON_400;
        case HTTP_STATUS_UNAUTHORIZED:
            return HTTP_REASON_401;
        case HTTP_STATUS_FORBIDDEN:
            return HTTP_REASON_403;
        case HTTP_STATUS_NOT_FOUND:
            return HTTP_REASON_404;
        case HTTP_STATUS_VERSION_NOT_SUPPORTED:
            return HTTP_REASON_505;
    }
    return "";
}
