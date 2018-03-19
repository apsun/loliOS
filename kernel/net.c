#include "net.h"
#include "lib.h"
#include "debug.h"

/* List of registered network interfaces */
static net_iface_t *interfaces[16];
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
