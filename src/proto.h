#ifndef _RA_PROTO_H
#define _RA_PROTO_H

#define LISTEN_PORT 21500

typedef enum {
    RA_UNKNOWN_MESSAGE_TYPE,
    RA_HANDSHAKE_INIT,
    RA_HANDSHAKE_RESPONSE,
    RA_MESSAGE_CRYPTO,
} ra_message_type;

typedef enum {
    RA_UNKNOWN_CRYPTO_TYPE,
    RA_STREAM_HEARTBEAT,
    RA_STREAM_DATA,
    RA_STREAM_TERMINATE,
} ra_crypto_type;

#endif
