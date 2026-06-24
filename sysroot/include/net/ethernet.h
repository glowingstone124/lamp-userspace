#ifndef _NET_ETHERNET_H
#define _NET_ETHERNET_H
#include <stdint.h>

#define ETH_ALEN       6
#define ETHERTYPE_IP   0x0800
#define ETHERTYPE_ARP  0x0806

struct ether_addr {
    uint8_t ether_addr_octet[ETH_ALEN];
};

#endif
