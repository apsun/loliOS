#include "ip.h"
#include "lib.h"
#include "debug.h"

/* Statically configured default gateway */
static ip_addr_t default_gateway = IP(10, 0, 2, 2);

/*
 * Finds an appropriate interface and IP address to send
 * the specified packet to. If the IP address does not
 * match any interface's subnet, then it will be replaced
 * with the default gateway's IP address. Returns the
 * interface to route the packet on.
 */
static net_iface_t *
ip_route(ip_addr_t *ip)
{
    /* Find an interface with subnet matching specified IP address */
    net_iface_t **ifaces;
    int num_ifaces = net_get_interfaces(&ifaces);
    int i;
    for (i = 0; i < num_ifaces; ++i) {
        net_iface_t *iface = ifaces[i];

        /* Check if (a & subnet_mask) == (b & subnet_mask) */
        uint32_t subnet_mask = iptoh(iface->subnet_mask);
        uint32_t dest_netaddr = iptoh(*ip) & subnet_mask;
        uint32_t iface_netaddr = iptoh(iface->ip_addr) & subnet_mask;
        if (dest_netaddr == iface_netaddr) {
            return iface;
        }
    }

    /* Didn't match any interfaces, route through default gateway */
    ASSERT(iptoh(*ip) != iptoh(default_gateway));
    *ip = default_gateway;
    return ip_route(ip);
}

/*
 * Handles an incoming IP packet.
 */
int
ip_handle_rx(net_iface_t *iface, skb_t *skb)
{
    // TODO
    return -1;
}

/*
 * Sends an IP packet to the specified IP address.
 */
int
ip_send(skb_t *skb, ip_addr_t ip)
{
    /* Determine interface and IP address for sending packet */
    ip_addr_t neigh_ip = ip;
    net_iface_t *iface = ip_route(&neigh_ip);
    if (!iface) {
        debugf("No interface to handle IP packet\n");
        return -1;
    }

    /* Prepend IP header */
    // ip_hdr_t *hdr = skb_push(skb, sizeof(ip_hdr_t));
    // TODO

    /* Forward to interface's IP packet handler */
    return iface->send_ip_skb(iface, skb, neigh_ip);
}
