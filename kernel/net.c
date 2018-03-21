#include "net.h"
#include "lib.h"
#include "debug.h"

/* List of registered network interfaces */
static net_iface_t *interfaces[16];
static int num_interfaces = 0;

/* Statically configured default gateway */
static ip_addr_t default_gateway = IP(10, 0, 2, 2);

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
 * Finds an appropriate interface and IP address to send
 * the specified packet to. If the IP address does not
 * match any interface's subnet, then it will be replaced
 * with the default gateway's IP address. Returns the
 * interface to route the packet on.
 */
net_iface_t *
net_route(ip_addr_t *ip)
{
    /* Find an interface with subnet matching specified IP address */
    int i;
    for (i = 0; i < num_interfaces; ++i) {
        net_iface_t *iface = interfaces[i];

        /* Check if (a & subnet_mask) == (b & subnet_mask) */
        uint32_t subnet_mask = iptoh(iface->subnet_mask);
        uint32_t iface_netaddr = iptoh(iface->ip_addr) & subnet_mask;
        uint32_t dest_netaddr = iptoh(*ip) & subnet_mask;
        if (dest_netaddr == iface_netaddr) {
            return iface;
        }
    }

    /* Didn't match any interfaces, route through default gateway */
    ASSERT(iptoh(*ip) != iptoh(default_gateway));
    *ip = default_gateway;
    return net_route(ip);
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
 * Gets a list of registered interfaces.
 * Returns the number of interfaces in the list.
 */
int
net_get_interfaces(net_iface_t ***out)
{
    *out = interfaces;
    return num_interfaces;
}

/*
 * Registers a new network interface.
 */
void
net_register_interface(net_iface_t *iface)
{
    ASSERT(num_interfaces < array_len(interfaces));
    interfaces[num_interfaces++] = iface;
}
