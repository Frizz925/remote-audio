#include "string.h"

int strequal(const char *c1, const char *c2) {
    return strcmp(c1, c2) == 0;
}

void strstrip(char *str) {
    char *p = str + strlen(str) - 1;
    for (; p > str; p--) {
        switch (*p) {
        case ' ':
        case '\r':
        case '\n':
            continue;
        }
        *(p + 1) = '\0';
        break;
    }
}
