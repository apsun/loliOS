#ifndef _IP_H
#define _IP_H

#include "net.h"
#include "skb.h"
#include "types.h"

#ifndef ASM

/* IP header */
typedef struct {
    uint8_t version : 4;
    uint8_t ihl : 4;
    uint8_t dscp : 6;
    uint8_t ecn : 2;
    uint16_t total_length;
    uint16_t identification;
    uint16_t : 1;
    uint16_t dont_fragment : 1;
    uint16_t more_fragments : 1;
    uint16_t flags : 3;
    uint16_t fragment_offset : 13;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t checksum;
    uint32_t src_ip;
    uint32_t dest_ip;
} ip_hdr_t;

/* Handles an incoming IP packet */
int ip_handle_rx(net_iface_t *iface, skb_t *skb);

/* Sends an IP packet */
int ip_send(skb_t *skb, ip_addr_t ip);

#endif /* ASM */

#endif /* _IP_H */
