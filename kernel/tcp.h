#ifndef _TCP_H
#define _TCP_H

#include "types.h"
#include "net.h"
#include "skb.h"

#ifndef ASM

/* Delivers all pending ACKs */
void tcp_deliver_ack(void);

/* Handles reception of a TCP packet */
int tcp_handle_rx(net_iface_t *iface, skb_t *skb);

/* Registers the TCP socket type */
void tcp_init(void);

#endif /* ASM */

#endif /* _TCP_H */
