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

    char nonce_bytes[NONCE_SIZE];
    randombytes_buf(nonce_bytes, NONCE_SIZE);
    uint64_to_bytes(nonce_bytes, ++stream->prev_nonce);

    char *p = outbuf;
    *p++ = (char)RA_STREAM_DATA;
    *p++ = (char)stream->id;
    memcpy(p, nonce_bytes, NONCE_SIZE);
    p += 2 + NONCE_SIZE;

    uint64_t encsize = outlen - (p - outbuf);
    int err = crypto_aead_xchacha20poly1305_ietf_encrypt((unsigned char *)p, &encsize, (unsigned char *)buf, len, NULL,
                                                         0, NULL, (unsigned char *)nonce_bytes, stream->secret);
    if (err) return err;
    uint16_to_bytes(p - 2, encsize);

    stream->buf.len = p + encsize - outbuf;
    return uv_udp_send(&stream->req, conn->sock, &stream->buf, 1, conn->addr, send_cb);
}

int ra_stream_read(ra_stream_t *stream, char *outbuf, size_t *outlen, const char *buf, size_t len) {
    const char *p = buf;

    const char *nonce_bytes = p;
    uint64_t nonce = bytes_to_uint64(nonce_bytes);
    if (nonce <= stream->prev_nonce) return -1;
    stream->prev_nonce = nonce;
    p += NONCE_SIZE;

    uint16_t msgsize = bytes_to_uint16(p);
    p += sizeof(uint16_t);

    return crypto_aead_xchacha20poly1305_ietf_decrypt((unsigned char *)outbuf, (unsigned long long *)outlen, NULL,
                                                      (unsigned char *)p, msgsize, NULL, 0,
                                                      (unsigned char *)nonce_bytes, stream->secret);
}
