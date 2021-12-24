#include "stream.h"

#include <stdatomic.h>

#include "proto.h"
#include "types.h"

void ra_stream_init(ra_stream_t *stream, uint8_t id) {
    stream->id = id;
    stream->buf.base = stream->rawbuf;
    stream->buf.len = 0;
    stream->state = 1;
}

int ra_stream_send(ra_stream_t *stream, const ra_conn_t *conn, const char *buf, size_t len, uv_udp_send_cb send_cb) {
    char *outbuf = stream->rawbuf;
    size_t outlen = sizeof(stream->rawbuf);

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

    stream->buf.len = wptr + encsize - outbuf;
    return uv_udp_send(&stream->req, conn->sock, &stream->buf, 1, conn->addr, send_cb);
}

int ra_stream_read(ra_stream_t *stream, char *outbuf, size_t *outlen, const char *buf, size_t len) {
    // Nonce + Payload size
    static const size_t hdrlen = NONCE_SIZE + sizeof(uint16_t);
    if (len < hdrlen) return -1;
    const char *rptr = buf;

    const char *nonce_bytes = rptr;
    uint64_t nonce = bytes_to_uint64(nonce_bytes);
    if (nonce <= stream->prev_nonce) return -1;
    stream->prev_nonce = nonce;
    rptr += NONCE_SIZE;

    uint16_t msgsize = bytes_to_uint16(rptr);
    rptr += sizeof(uint16_t);

    return crypto_aead_xchacha20poly1305_ietf_decrypt((unsigned char *)outbuf, (unsigned long long *)outlen, NULL,
                                                      (unsigned char *)rptr, msgsize, NULL, 0,
                                                      (unsigned char *)nonce_bytes, stream->secret);
}
