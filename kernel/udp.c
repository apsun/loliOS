#include "udp.h"
#include "ip.h"

/* Used for checksum calculation */
typedef struct {
    ip_addr_t src_ip;
    ip_addr_t dest_ip;
    uint8_t zero;
    uint8_t protocol;
    uint16_t be_udp_length;
    uint16_t be_src_port;
    uint16_t be_dest_port;
    uint16_t length;
    uint16_t checksum;
} udp_pseudohdr_t;

int
udp_handle_rx(net_iface_t *iface, skb_t *skb)
{
    return 0;
}

int
udp_send(net_sock_t *sock, skb_t *skb, ip_addr_t ip, int port)
{
    udp_hdr_t *hdr = skb_push(skb, sizeof(udp_hdr_t));
    hdr->be_src_port = htons(sock->port);
    hdr->be_dest_port = htons(port);
    hdr->be_length = htons(0);
    hdr->be_checksum = htons(0);
    return ip_send(sock->iface, skb, ip, IPPROTO_UDP);
}

int
udp_bind(net_sock_t *sock, const sock_addr_t *addr)
{
    return -1;
}

int
udp_recvfrom(net_sock_t *sock, void *buf, int nbytes, sock_addr_t *addr)
{
    return -1;
}

int
udp_sendto(net_sock_t *sock, const void *buf, int nbytes, const sock_addr_t *addr)
{
    return -1;
}
