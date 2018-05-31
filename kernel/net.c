#include "net.h"
#include "lib.h"
#include "debug.h"

/* List of registered network interfaces */
static net_iface_t *interfaces[2];
static int num_interfaces = 0;

/*
 * Returns the interface corresponding to the specified
 * device. Since we don't support VLANs, there should be
 * at most one interface per device.
 */
net_iface_t *
net_get_interface(net_dev_t *dev)
{
    int i;
    for (i = 0; i < array_len(interfaces); ++i) {
        net_iface_t *iface = interfaces[i];
        if (iface->dev == dev) {
            return iface;
        }
    }
    return NULL;
}

/*
 * Returns whether the specified IP address is in the same
 * subnet as an interface.
 */
static bool
net_in_subnet(net_iface_t *iface, ip_addr_t ip)
{
    /* Check if (a & subnet_mask) == (b & subnet_mask) */
    uint32_t subnet_mask = iptoh(iface->subnet_mask);
    uint32_t iface_netaddr = iptoh(iface->ip_addr) & subnet_mask;
    uint32_t dest_netaddr = iptoh(ip) & subnet_mask;
    return dest_netaddr == iface_netaddr;
}

/*
 * Finds an appropriate interface and IP address to send
 * an IP packet to. If iface is not NULL, the packet will
 * be forced to route through it. Otherwise, an interface
 * will be chosen (currently by first match). neigh_ip will be
 * set to the IP of the next-hop, and the interface that
 * the packet will be routed on is returned. If the packet
 * cannot be routed, NULL is returned.
 */
net_iface_t *
net_route(net_iface_t *iface, ip_addr_t ip, ip_addr_t *neigh_ip)
{
    /* If interface specified, must route through it */
    net_iface_t **iface_list;
    int iface_count;
    if (iface != NULL) {
        iface_list = &iface;
        iface_count = 1;
    } else {
        iface_list = interfaces;
        iface_count = num_interfaces;
    }

    /* Find an interface with subnet matching specified IP address */
    int i;
    for (i = 0; i < iface_count; ++i) {
        net_iface_t *iface = iface_list[i];
        if (net_in_subnet(iface, ip)) {
            *neigh_ip = ip;
            return iface;
        }
    }

    /* No matching subnets? Okay, then route it through a gateway */
    for (i = 0; i < iface_count; ++i) {
        net_iface_t *iface = iface_list[i];
        if (!ip_equals(iface->gateway_addr, INVALID_IP)) {
            *neigh_ip = iface->gateway_addr;
            return iface;
        }
    }

    /* No gateways available */
    return NULL;
}

/*
 * Returns the interface with the specified IP address,
 * or null if no interfaces have that address.
 */
net_iface_t *
net_find(ip_addr_t ip)
{
    int i;
    for (i = 0; i < num_interfaces; ++i) {
        net_iface_t *iface = interfaces[i];
        if (ip_equals(iface->ip_addr, ip)) {
            return iface;
        }
    }
    return NULL;
}

/*
 * Registers a new network interface.
 */
void
net_register_interface(net_iface_t *iface)
{
    assert(num_interfaces < array_len(interfaces));
    interfaces[num_interfaces++] = iface;
}
