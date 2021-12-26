#include "stream.h"

#include <stdatomic.h>

#include "proto.h"
#include "string.h"
#include "types.h"

void ra_buf_init(ra_buf_t *buf, char *rawbuf, size_t size) {
    buf->base = rawbuf;
    buf->len = 0;
    buf->cap = size;
}

void ra_rbuf_init(ra_rbuf_t *buf, const char *rawbuf, size_t len) {
    buf->base = rawbuf;
    buf->len = len;
}

ssize_t ra_buf_send(const ra_conn_t *conn, const ra_rbuf_t *buf) {
    ssize_t res = sendto(conn->sock, buf->base, buf->len, 0, conn->addr, sizeof(struct sockaddr));
    if (res < 0) perror("sendto");
    return res;
}

ra_stream_t *ra_stream_create(uint8_t id, size_t bufsize) {
    ra_buf_t *sbuf = malloc(sizeof(ra_buf_t));
    ra_buf_init(sbuf, malloc(bufsize), bufsize);

    ra_stream_t *stream = malloc(sizeof(ra_stream_t));
    ra_stream_init(stream, id, sbuf);
    return stream;
}

void ra_stream_init(ra_stream_t *stream, uint8_t id, ra_buf_t *buf) {
    stream->id = id;
    stream->buf = buf;
    ra_stream_reset(stream);
}

void ra_stream_reset(ra_stream_t *stream) {
    stream->prev_nonce = 0;
}

ssize_t ra_stream_send(ra_stream_t *stream, const ra_conn_t *conn, const char *buf, size_t len) {
    char *outbuf = stream->buf->base;
    size_t outlen = stream->buf->cap;

    char *wptr = outbuf;
    *wptr++ = (char)RA_MESSAGE_CRYPTO;
    *wptr++ = (char)stream->id;

    char *nonce_bytes = wptr;
    randombytes_buf(nonce_bytes, NONCE_SIZE);
    uint64_to_bytes(nonce_bytes, ++stream->prev_nonce);
    wptr += NONCE_SIZE;

    char *szptr = wptr;
    wptr += sizeof(uint16_t);

    uint64_t encsize = outlen - (wptr - outbuf);
    int err = crypto_aead_xchacha20poly1305_ietf_encrypt((unsigned char *)wptr, &encsize, (unsigned char *)buf, len,
                                                         NULL, 0, NULL, (unsigned char *)nonce_bytes, stream->secret);
    if (err) return err;
    uint16_to_bytes(szptr, encsize);

    stream->buf->len = wptr + encsize - outbuf;
    return ra_buf_send(conn, (ra_rbuf_t *)stream->buf);
}

int ra_stream_read(ra_stream_t *stream, ra_buf_t *buf, const char *inbuf, size_t len) {
    // Nonce + Payload size
    static const size_t hdrlen = NONCE_SIZE + sizeof(uint16_t);
    if (len < hdrlen) return -1;
    const char *rptr = inbuf;

    const char *nonce_bytes = rptr;
    uint64_t nonce = bytes_to_uint64(nonce_bytes);
    if (nonce <= stream->prev_nonce) return -1;
    stream->prev_nonce = nonce;
    rptr += NONCE_SIZE;

    uint16_t msgsize = bytes_to_uint16(rptr);
    rptr += sizeof(uint16_t);

    buf->len = buf->cap;
    return crypto_aead_xchacha20poly1305_ietf_decrypt((unsigned char *)buf->base, (unsigned long long *)&buf->len, NULL,
                                                      (unsigned char *)rptr, msgsize, NULL, 0,
                                                      (unsigned char *)nonce_bytes, stream->secret);
}

void ra_stream_destroy(ra_stream_t *stream) {
    free(stream->buf->base);
    free(stream->buf);
    free(stream);
}
