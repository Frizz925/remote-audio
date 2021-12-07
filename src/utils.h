#ifndef _H_UTILS
#define _H_UTILS

#include <arpa/inet.h>

int straddr_p(struct sockaddr_in *saddr, char *buf);
const char *straddr(struct sockaddr_in *saddr);
#endif
