#include "ethernet.h"
#include "lib.h"
#include "debug.h"

int
ethernet_handle_rx(skb_t *skb)
{
    printf("Ethernet RX\n");
    return 0;
}

int
ethernet_handle_tx(skb_t *skb)
{
    printf("Ethernet TX\n");
    return 0;
}
