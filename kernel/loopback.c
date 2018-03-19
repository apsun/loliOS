#include "loopback.h"
#include "lib.h"
#include "debug.h"
#include "net.h"
#include "ip.h"

/*
 * Loopback "send" function - just redirects the packet
 * to the IP rx handler.
 */
static int
loopback_send(net_iface_t *iface, skb_t *skb, ip_addr_t ip)
{
    // TODO: Should we clone the SKB?
    debugf("Received loopback packet\n");
    return ip_handle_rx(iface, skb);
}

/* Loopback interface */
static net_iface_t lo = {
    .name = "lo",
    .subnet_mask = IP(255, 0, 0, 0),
    .ip_addr = IP(127, 0, 0, 1),
    .dev = NULL,
    .send_ip_skb = loopback_send,
};

/* Initializes the loopback interface */
void
loopback_init(void)
{
    net_register_interface(&lo);
}
