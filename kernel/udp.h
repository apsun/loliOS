#ifndef _UDP_H
#define _UDP_H

#include "types.h"
#include "net.h"
#include "skb.h"
#include "socket.h"

#ifndef ASM

/* UDP header structure */
typedef struct {
    uint16_t be_src_port;
    uint16_t be_dest_port;
    uint16_t be_length;
    uint16_t be_checksum;
} udp_hdr_t;

/* Handles reception of a UDP datagram */
int udp_handle_rx(net_iface_t *iface, skb_t *skb);

/* Sends a UDP datagram to the specified IP and port */
int udp_send(net_sock_t *sock, skb_t *skb, ip_addr_t ip, int port);

/* Socket syscall handlers */
int udp_socket(net_sock_t *sock);
int udp_bind(net_sock_t *sock, const sock_addr_t *addr);
int udp_recvfrom(net_sock_t *sock, void *buf, int nbytes, sock_addr_t *addr);
int udp_sendto(net_sock_t *sock, const void *buf, int nbytes, const sock_addr_t *addr);
int udp_close(net_sock_t *sock);

#endif /* ASM */

#endif /* _UDP_H */
