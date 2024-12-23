#ifndef _IP_H
#define _IP_H

#include "net.h"
#include "skb.h"
#include "types.h"

#ifndef ASM

/* IP header */
typedef struct {
    uint8_t ihl : 4;
    uint8_t version : 4;
    uint8_t tos;
    be16_t be_total_length;
    be16_t be_identification;
    be16_t be_flags;
    uint8_t ttl;
    uint8_t protocol;
    be16_t be_checksum;
    ip_addr_t src_ip;
    ip_addr_t dest_ip;
} __packed ip_hdr_t;

/* IP protocol identifier constants */
typedef enum {
    IPPROTO_ICMP = 0x01,
    IPPROTO_TCP = 0x06,
    IPPROTO_UDP = 0x11,
} ipproto_t;

/* Computes a TCP or UDP checksum */
be16_t ip_pseudo_checksum(
    skb_t *skb,
    ip_addr_t src_ip,
    ip_addr_t dest_ip,
    ipproto_t protocol);

/* Handles an incoming IP packet */
int ip_handle_rx(net_iface_t *iface, skb_t *skb);

/* Sends an IP packet */
int ip_send(
    net_iface_t *iface,
    ip_addr_t neigh_ip,
    skb_t *skb,
    ip_addr_t dest_ip,
    ipproto_t protocol);

#endif /* ASM */

#endif /* _IP_H */
