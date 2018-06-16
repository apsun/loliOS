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
    /*
     * Cloning the SKB is necessary, since SKBs may only be in
     * one queue at a time, and TCP requires both an inbox and
     * outbox queue.
     */
    return ip_handle_rx(iface, skb_clone(skb));
}

/* Loopback interface */
static net_iface_t lo = {
    .name = "lo",
    .subnet_mask = IP(255, 0, 0, 0),
    .ip_addr = IP(127, 0, 0, 1),
    .gateway_addr = INVALID_IP,
    .dev = NULL,
    .send_ip_skb = loopback_send,
};

/* Initializes the loopback interface */
void
loopback_init(void)
{
    net_register_interface(&lo);
}
