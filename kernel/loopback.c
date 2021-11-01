#include "loopback.h"
#include "types.h"
#include "debug.h"
#include "list.h"
#include "net.h"
#include "ip.h"
#include "skb.h"

/* Packets that are waiting to be sent */
static list_define(loopback_queue);

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
    assert(skb_mac_header(skb) == NULL);

    /*
     * You may be wondering, why can't we just deliver the packet
     * now? The problem is that our networking code is not re-entrant.
     * If our rx handler sends a packet, we end up having a nested
     * call to the rx handler, and this causes huge problems. Even
     * if we made the code re-entrant (incredibly difficult), we would
     * still run the risk of overflowing our stack.
     */
    skb_t *clone = skb_clone(skb);
    if (clone == NULL) {
        debugf("Failed to clone SKB\n");
        return -1;
    }

    skb_clear_network_header(clone);
    skb_clear_transport_header(clone);
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
