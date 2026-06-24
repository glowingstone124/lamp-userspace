#ifndef LAMP_LIBC_NETINET_IP_ICMP_H
#define LAMP_LIBC_NETINET_IP_ICMP_H

#include <stdint.h>
#include <netinet/in.h>

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define LAMP_IPHDR_IHL_VERSION unsigned int ihl:4, version:4
#else
#define LAMP_IPHDR_IHL_VERSION unsigned int version:4, ihl:4
#endif

struct iphdr {
    LAMP_IPHDR_IHL_VERSION;
    uint8_t tos;
    uint16_t tot_len;
    uint16_t id;
    uint16_t frag_off;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t check;
    uint32_t saddr;
    uint32_t daddr;
};

#undef LAMP_IPHDR_IHL_VERSION

#define ICMP_ECHOREPLY 0
#define ICMP_DEST_UNREACH 3
#define ICMP_SOURCE_QUENCH 4
#define ICMP_REDIRECT 5
#define ICMP_ECHO 8
#define ICMP_TIME_EXCEEDED 11
#define ICMP_PARAMETERPROB 12
#define ICMP_TIMESTAMP 13
#define ICMP_TIMESTAMPREPLY 14
#define ICMP_INFO_REQUEST 15
#define ICMP_INFO_REPLY 16
#define ICMP_ADDRESS 17
#define ICMP_ADDRESSREPLY 18

#define ICMP_MINLEN 8

struct icmp {
    uint8_t icmp_type;
    uint8_t icmp_code;
    uint16_t icmp_cksum;
    union {
        struct {
            uint16_t id;
            uint16_t seq;
        } echo;
        uint32_t gateway;
    } icmp_hun;
    union {
        uint8_t data[1];
        uint32_t ts;
    } icmp_dun;
};

#define icmp_id icmp_hun.echo.id
#define icmp_seq icmp_hun.echo.seq
#define icmp_data icmp_dun.data

#endif
