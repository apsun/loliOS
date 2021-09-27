#ifndef _UDP_H
#define _UDP_H

#include "types.h"
#include "net.h"
#include "skb.h"
#include "socket.h"

#ifndef ASM

/* UDP header structure */
typedef struct {
    be16_t be_src_port;
    be16_t be_dest_port;
    be16_t be_length;
    be16_t be_checksum;
} __packed udp_hdr_t;

/* Handles reception of a UDP datagram */
int udp_handle_rx(net_iface_t *iface, skb_t *skb);

/* Socket syscall handlers */
int udp_ctor(net_sock_t *sock);
void udp_dtor(net_sock_t *sock);
int udp_bind(net_sock_t *sock, const sock_addr_t *addr);
int udp_connect(net_sock_t *sock, const sock_addr_t *addr);
int udp_recvfrom(net_sock_t *sock, void *buf, int nbytes, sock_addr_t *addr);
int udp_sendto(net_sock_t *sock, const void *buf, int nbytes, const sock_addr_t *addr);

#endif /* ASM */

#endif /* _UDP_H */
