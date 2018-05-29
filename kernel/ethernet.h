#ifndef _ETHERNET_H
#define _ETHERNET_H

#include "net.h"
#include "skb.h"
#include "types.h"

#define ETHERTYPE_IPV4 0x0800
#define ETHERTYPE_ARP 0x0806

/* Maximum length of an Ethernet frame body */
#define ETHERNET_MAX_LEN 1500

#ifndef ASM

/* Ethernet header */
typedef struct {
    mac_addr_t dest_addr;
    mac_addr_t src_addr;
    uint16_t be_ethertype;
} __packed ethernet_hdr_t;

/* Handles reception of an Ethernet frame */
int ethernet_handle_rx(net_dev_t *dev, skb_t *skb);

/* Sends an Ethernet frame */
int ethernet_send_mac(net_dev_t *dev, skb_t *skb, mac_addr_t mac, int ethertype);

/* Sends an IP-over-Ethernet frame */
int ethernet_send_ip(net_iface_t *iface, skb_t *skb, ip_addr_t ip);

#endif /* ASM */

#endif /* _ETHERNET_H */
