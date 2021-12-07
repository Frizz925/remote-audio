#include "message.h"

int message_format_type(char *buf, MessageType type) {
    *buf = type;
    return 1;
}

int message_parse_type(const char *buf, MessageType *result) {
    *result = buf[0];
    return 1;
}
