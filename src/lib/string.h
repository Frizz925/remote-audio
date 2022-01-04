#ifndef _RA_STRING_H
#define _RA_STRING_H

#include <string.h>

#ifdef _WIN32
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#endif

inline int strequal(const char *c1, const char *c2) {
    return strcmp(c1, c2) == 0;
}

#endif
