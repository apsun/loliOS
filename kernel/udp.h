#ifndef _UDP_H
#define _UDP_H

#include "types.h"
#include "net.h"
#include "skb.h"

#ifndef ASM

/* Handles reception of a UDP datagram */
int udp_handle_rx(net_iface_t *iface, skb_t *skb);

/* Registers the UDP socket type */
void udp_init(void);

#endif /* ASM */

#endif /* _UDP_H */
