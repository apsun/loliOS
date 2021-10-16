#include "ethernet.h"
#include "types.h"
#include "debug.h"
#include "net.h"
#include "arp.h"
#include "ip.h"

/*
 * Handles an incoming Ethernet frame.
 */
int
ethernet_handle_rx(net_dev_t *dev, skb_t *skb)
{
    /* Check packet size */
    if (!skb_may_pull(skb, sizeof(ethernet_hdr_t))) {
        debugf("Ethernet frame too small\n");
        return -1;
    }

    /* Pop Ethernet header */
    ethernet_hdr_t *hdr = skb_set_mac_header(skb);
    skb_pull(skb, sizeof(ethernet_hdr_t));

    /* Only handle IPv4 and ARP packets */
    switch (ntohs(hdr->be_ethertype)) {
    case ETHERTYPE_IPV4:
        return ip_handle_rx(net_get_interface(dev), skb);
    case ETHERTYPE_ARP:
        return arp_handle_rx(dev, skb);
    default:
        debugf("Unknown packet ethertype\n");
        return -1;
    }
}

/*
 * Sends an Ethernet packet to the neighbor with the
 * specified MAC address.
 */
int
ethernet_send_mac(net_dev_t *dev, skb_t *skb, mac_addr_t mac, ethertype_t ethertype)
{
    assert(skb_mac_header(skb) == NULL);
    ethernet_hdr_t *hdr = skb_push(skb, sizeof(ethernet_hdr_t));
    skb_set_mac_header(skb);
    hdr->dest_addr = mac;
    hdr->src_addr = dev->mac_addr;
    hdr->be_ethertype = htons(ethertype);

    int ret = dev->send_mac_skb(dev, skb);
    skb_clear_mac_header(skb);
    skb_pull(skb, sizeof(ethernet_hdr_t));
    return ret;
}

/*
 * Sends an IP-over-Ethernet packet to the neighbor
 * with the specified IP address. This will perform
 * ARP resolution. If the MAC address is known, call
 * ethernet_send_mac() directly.
 */
int
ethernet_send_ip(net_iface_t *iface, skb_t *skb, ip_addr_t ip)
{
    net_dev_t *dev = iface->dev;
    mac_addr_t mac;
    switch (arp_get_state(dev, ip, &mac)) {
    case ARP_INVALID:
        if (arp_send_request(iface, ip) < 0) {
            return -1;
        }
        __fallthrough;
    case ARP_WAITING:
        return arp_queue_insert(dev, ip, skb);
    case ARP_UNREACHABLE:
        debugf("Destination unreachable, dropping packet\n");
        return -1;
    case ARP_REACHABLE:
        return ethernet_send_mac(dev, skb, mac, ETHERTYPE_IPV4);
    default:
        panic("Unknown ARP state\n");
        return -1;
    }
}
