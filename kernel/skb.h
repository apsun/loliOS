#ifndef _SKB_H
#define _SKB_H

#include "types.h"

#ifndef ASM

/*
 * Socket kernel buffer structure, just like the one in Linux.
 */
typedef struct {
    uint16_t used; /* Must be first 2B, to ensure IP header alignment */
    uint8_t buf[1518]; /* Maximum size of Ethernet frame */
    uint8_t *head;
    uint8_t *data;
    uint8_t *tail;
    uint8_t *end;
    int len;
    void *mac_header;
    void *network_header;
    void *transport_header;
#if 0
    union {
        void *hdr2;
        eth_hdr_t *eth_hdr;
    };
    union {
        void *hdr3;
        ip_hdr_t *ip_hdr;
        arp_hdr_t *arp_hdr;
    };
    union {
        void *hdr4;
        udp_hdr_t *udp_hdr;
        tcp_hdr_t *tcp_hdr;
    };
#endif
} skb_t;

/* Allocates a new SKB */
skb_t *skb_alloc(void);

/* Pushes data at the start the data section */
void *skb_push(skb_t *skb, int len);

/* Pops data at the start of the data section */
void *skb_pull(skb_t *skb, int len);

/* Appends data to the end of the data section */
void *skb_put(skb_t *skb, int len);

/* Reserves space for the head section */
void skb_reserve(skb_t *skb, int len);

/* Releases an allocated SKB */
void skb_free(skb_t *skb);

#endif /* ASM */

#endif /* _SKB_H */
