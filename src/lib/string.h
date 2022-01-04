#ifndef _RA_STRING_H
#define _RA_STRING_H

#include <string.h>

#ifdef _WIN32
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#endif

#endif
