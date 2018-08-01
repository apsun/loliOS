#ifndef _ARP_H
#define _ARP_H

#include "net.h"
#include "skb.h"

#ifndef ASM

/* Possible states of a neighbor */
typedef enum {
    ARP_INVALID,     /* No entry in cache */
    ARP_WAITING,     /* Waiting for reply */
    ARP_UNREACHABLE, /* No reply received */
    ARP_REACHABLE,   /* Reply received */
} arp_state_t;

/* Resolves an IP address to a MAC address */
arp_state_t arp_get_state(net_dev_t *dev, ip_addr_t ip, mac_addr_t *mac);

/* Sends an ARP request for the specified IP address */
int arp_send_request(net_iface_t *iface, ip_addr_t ip);

/* Enqueues an Ethernet frame for transmission once ip is resolved */
int arp_queue_insert(net_dev_t *dev, ip_addr_t ip, skb_t *skb);

/* Handles an ARP packet */
int arp_handle_rx(net_dev_t *dev, skb_t *skb);

#endif /* ASM */

#endif /* _ARP_H */
