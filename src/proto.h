#ifndef _RA_PROTO_H
#define _RA_PROTO_H

#define LISTEN_PORT 21500

typedef enum {
    RA_UNKNOWN,
    RA_STREAM_INIT,
    RA_STREAM_INIT_RESPONSE,
    RA_STREAM_HEARTBEAT,
    RA_STREAM_DATA,
    RA_STREAM_TERMINATE,
} ra_message_type;

#endif
