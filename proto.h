enum PacketType {
    PacketTypeHandshake = 1,
    PacketTypeStream
};

enum PacketFlag {
    PacketFlagNone = 0,
    PacketFlagSYN = 1 << 0,
    PacketFlagACK = 1 << 1,
    PacketFlagRST = 1 << 3,
    PacketFlagFIN = 1 << 4,
};

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned long uint32;
typedef unsigned long uint64;

typedef char int8;
typedef short int16;
typedef long int32;
typedef long int64;

typedef unsigned long keylen_t;

int proto_write_header(char *stream, int type, int flags);
int proto_read_header(const char *stream, int *type, int *flags);

int proto_write_handshake_init(char *stream, const void *pubkey, keylen_t keylen);
int proto_read_handshake_init(const char *stream, void **pubkey, keylen_t *keylen);

int proto_write_handshake_resp(char *stream, const void *pubkey, keylen_t keylen);
int proto_read_handshake_resp(const char *stream, void **pubkey, keylen_t *keylen);
