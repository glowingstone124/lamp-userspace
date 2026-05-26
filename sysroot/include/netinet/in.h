#ifndef LAMP_LIBC_NETINET_IN_H
#define LAMP_LIBC_NETINET_IN_H

#include <sys/socket.h>

#define IPPROTO_IP 0
#define IPPROTO_ICMP 1
#define IPPROTO_TCP 6
#define IPPROTO_UDP 17
#define IPPROTO_RAW 255

#define IP_TTL 2
#define IP_MULTICAST_IF 32
#define IP_MULTICAST_TTL 33

#endif
