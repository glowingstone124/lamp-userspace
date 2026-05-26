#ifndef LAMP_LIBC_ARPA_INET_H
#define LAMP_LIBC_ARPA_INET_H

#include <sys/socket.h>

#define INET_ADDRSTRLEN 16
#define INET6_ADDRSTRLEN 46

unsigned short htons(unsigned short x);
unsigned short ntohs(unsigned short x);
unsigned int htonl(unsigned int x);
unsigned int ntohl(unsigned int x);
const char *inet_ntop(int af, const void *src, char *dst, socklen_t size);
int inet_pton(int af, const char *src, void *dst);

#endif
