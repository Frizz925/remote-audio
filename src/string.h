#ifndef _RA_STRING_H
#define _RA_STRING_H

#ifdef _WIN32
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#else
#include <string.h>
#endif

#endif
