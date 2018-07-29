#include "loopback.h"
#include "lib.h"
#include "debug.h"
#include "net.h"
#include "ip.h"

/* Packets that are waiting to be sent */
static list_declare(loopback_queue);

/* Forward declaration */
static int loopback_send(net_iface_t *iface, skb_t *skb, ip_addr_t ip);

/* Loopback interface */
static net_iface_t lo = {
    .name = "lo",
    .subnet_mask = IP(255, 0, 0, 0),
    .ip_addr = IP(127, 0, 0, 1),
    .gateway_addr = INVALID_IP,
    .dev = NULL,
    .send_ip_skb = loopback_send,
};

/*
 * Loopback "send" function - just redirects the packet
 * to the IP rx handler. Packets will be delivered at the
 * end of the current interrupt.
 */
static int
loopback_send(net_iface_t *iface, skb_t *skb, ip_addr_t ip)
{
    skb_t *clone = skb_clone(skb);
    list_add_tail(&clone->list, &loopback_queue);
    return 0;
}

/*
 * Delivers any queued loopback packets. Called at the end of
 * every interrupt.
 */
void
loopback_deliver(void)
{
    while (!list_empty(&loopback_queue)) {
        skb_t *pending = list_first_entry(&loopback_queue, skb_t, list);
        list_del(&pending->list);
        ip_handle_rx(&lo, pending);
        skb_release(pending);
    }
}

/* Initializes the loopback interface */
void
loopback_init(void)
{
    net_register_interface(&lo);
}
