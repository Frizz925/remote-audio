#ifndef _H_MESSAGE
#define _H_MESSAGE

typedef enum {
    MESSAGE_TYPE_UNKNOWN,
    MESSAGE_TYPE_HANDSHAKE_INIT,
    MESSAGE_TYPE_HANDSHAKE_RESPONSE,
    MESSAGE_TYPE_STREAM,
    MESSAGE_TYPE_TERMINATE,
} MessageType;

int message_format_type(char *buf, MessageType type);
int message_parse_type(const char *buf, MessageType *result);
#endif
