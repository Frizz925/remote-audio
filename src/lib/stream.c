#include "stream.h"

#include <stdatomic.h>

#include "proto.h"
#include "socket.h"
#include "string.h"
#include "types.h"

#define HEADER_SIZE NONCE_SIZE + 2
#define WINDOW_SIZE 32

ra_stream_t *ra_stream_create(uint8_t id) {
    ra_stream_t *stream = malloc(sizeof(ra_stream_t));
    ra_stream_init(stream, id);
    return stream;
}

void ra_stream_init(ra_stream_t *stream, uint8_t id) {
    stream->id = id;
    ra_stream_reset(stream);
}

void ra_stream_reset(ra_stream_t *stream) {
    stream->read_nonce = 0;
    stream->write_nonce = 0;
}

int ra_stream_write(ra_stream_t *stream, char *outbuf, size_t *outlen, const ra_rbuf_t *buf) {
    if (*outlen < HEADER_SIZE) return -1;
    char *wptr = outbuf;
    char *endptr = wptr + *outlen;

    char *nonce_bytes = wptr;
    randombytes_buf(nonce_bytes, NONCE_SIZE);
    uint64_to_bytes(nonce_bytes, ++stream->write_nonce);
    wptr += NONCE_SIZE;

    char *szptr = wptr;
    wptr += sizeof(uint16_t);

    uint64_t sz_payload = endptr - wptr;
    int err = crypto_aead_xchacha20poly1305_ietf_encrypt((unsigned char *)wptr, &sz_payload, (unsigned char *)buf->base,
                                                         buf->len, NULL, 0, NULL, (unsigned char *)nonce_bytes,
                                                         stream->secret);
    if (err) return err;
    uint16_to_bytes(szptr, sz_payload);

    *outlen = wptr + sz_payload - outbuf;
    return 0;
}

int ra_stream_read(ra_stream_t *stream, ra_buf_t *buf, const char *inbuf, size_t len) {
    if (len < HEADER_SIZE) return -1;
    const char *rptr = inbuf;

    const char *nonce_bytes = rptr;
    uint64_t read_nonce = stream->read_nonce;
    uint64_t nonce = bytes_to_uint64(nonce_bytes);
    if (nonce + WINDOW_SIZE < read_nonce) return -1;
    rptr += NONCE_SIZE;

    uint16_t sz_payload = bytes_to_uint16(rptr);
    rptr += sizeof(uint16_t);

    buf->len = buf->cap;
    int err = crypto_aead_xchacha20poly1305_ietf_decrypt((unsigned char *)buf->base, (unsigned long long *)&buf->len,
                                                         NULL, (unsigned char *)rptr, sz_payload, NULL, 0,
                                                         (unsigned char *)nonce_bytes, stream->secret);
    if (err) return err;
    if (nonce > read_nonce) stream->read_nonce = nonce;
    return 0;
}

ssize_t ra_stream_send(ra_stream_t *stream, const ra_conn_t *conn, const ra_rbuf_t *buf) {
    char rawbuf[BUFSIZE];

    char *wptr = rawbuf;
    char *endptr = wptr + BUFSIZE;
    *wptr++ = (char)RA_MESSAGE_CRYPTO;
    *wptr++ = (char)stream->id;

    size_t sz_write = endptr - wptr;
    int err = ra_stream_write(stream, wptr, &sz_write, buf);
    if (err) return err;

    ra_rbuf_t rbuf = {.base = rawbuf, .len = wptr - rawbuf + sz_write};
    return ra_buf_sendto(conn, &rbuf);
}

void ra_stream_destroy(ra_stream_t *stream) {
    free(stream);
}
