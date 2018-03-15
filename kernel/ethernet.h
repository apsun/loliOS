#ifndef _ETHERNET_H
#define _ETHERNET_H

#include "skb.h"

#ifndef ASM

/* Handles reception of an Ethernet frame */
int ethernet_handle_rx(skb_t *skb);

/* Handles transmission of an Ethernet frame */
int ethernet_handle_tx(skb_t *skb);

#endif /* ASM */

#endif /* _ETHERNET_H */
