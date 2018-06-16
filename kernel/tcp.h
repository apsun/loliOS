#ifndef _TCP_H
#define _TCP_H

#include "types.h"
#include "net.h"
#include "skb.h"
#include "socket.h"

#ifndef ASM

/* TCP header structure */
typedef struct {
    uint16_t be_src_port;
    uint16_t be_dest_port;
    uint32_t be_seq_num;
    uint32_t be_ack_num;
    uint16_t ns : 1;
    uint16_t reserved : 3;
    uint16_t data_offset : 4;
    uint16_t fin : 1;
    uint16_t syn : 1;
    uint16_t rst : 1;
    uint16_t psh : 1;
    uint16_t ack : 1;
    uint16_t urg : 1;
    uint16_t ece : 1;
    uint16_t cwr : 1;
    uint16_t be_window_size;
    uint16_t be_checksum;
    uint16_t be_urg_ptr;
} tcp_hdr_t;

/* Handles reception of a TCP packet */
int tcp_handle_rx(net_iface_t *iface, skb_t *skb);

/* Socket syscall handlers */
int tcp_ctor(net_sock_t *sock);
void tcp_dtor(net_sock_t *sock);
int tcp_bind(net_sock_t *sock, const sock_addr_t *addr);
int tcp_connect(net_sock_t *sock, const sock_addr_t *addr);
int tcp_listen(net_sock_t *sock, int backlog);
int tcp_accept(net_sock_t *sock, sock_addr_t *addr);
int tcp_recvfrom(net_sock_t *sock, void *buf, int nbytes, sock_addr_t *addr);
int tcp_sendto(net_sock_t *sock, const void *buf, int nbytes, const sock_addr_t *addr);
int tcp_close(net_sock_t *sock);

#endif /* ASM */

#endif /* _TCP_H */
