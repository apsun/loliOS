#include "tcp.h"
#include "lib.h"
#include "debug.h"
#include "syscall.h"
#include "myalloc.h"
#include "socket.h"
#include "paging.h"
#include "net.h"
#include "ip.h"
#include "ethernet.h"

/*
 * WARNING: This TCP implementation is not RFC793-compliant!
 * It is designed under the assumption that it is running under
 * QEMU, and hence most of the ugly work is handled by the
 * host. Unless your network is perfect, you should not use
 * this code!
 */
typedef struct {
    list_t backlog;

    list_t inbox;
    list_t outbox;
    int inbox_size;
    int outbox_size;

    /*
     * Sequence number: in bytes, represents the number of the
     * NEXT byte that we expect to receive.
     */
    uint32_t local_seq;
    uint32_t remote_ack;

    /*
     * Congestion window: how much more is this connection able
     * to receive before the buffer is full?
     */
    uint32_t local_cwnd;
    uint32_t remote_cwnd;

    bool should_send_ack : 1;
    bool should_send_syn : 1;
} tcp_sock_t;

/*
 * Handles an incoming SYN to a listening socket.
 */
static int
tcp_handle_syn(net_sock_t *sock, skb_t *skb)
{
    /* TODO */
    debugf("tcp_handle_syn()\n");
    return -1;
}

/*
 * Handles an incoming packet to a connected socket.
 */
static int
tcp_handle_pkt(net_sock_t *sock, skb_t *skb)
{
    /* TODO */
    debugf("tcp_handle_pkt()\n");
    return -1;
}

/* Handles reception of a TCP packet */
int
tcp_handle_rx(net_iface_t *iface, skb_t *skb)
{
    /* Check packet size */
    if (!skb_may_pull(skb, sizeof(tcp_hdr_t))) {
        debugf("TCP packet too small\n");
        return -1;
    }

    /* Pop TCP header */
    ip_hdr_t *iphdr = skb_network_header(skb);
    tcp_hdr_t *hdr = skb_reset_transport_header(skb);
    skb_pull(skb, sizeof(tcp_hdr_t));

    /* TODO: logging code */
    debugf("syn=%d, ack=%d, src=%d, dest=%d\n",
        hdr->syn, hdr->ack, ntohs(hdr->be_src_port), ntohs(hdr->be_dest_port));

    /* Try to dispatch to a connected socket */
    net_sock_t *sock = get_sock_by_addr(SOCK_TCP,
        iphdr->dest_ip, ntohs(hdr->be_dest_port),
        iphdr->src_ip, ntohs(hdr->be_src_port));
    if (sock != NULL) {
        return tcp_handle_pkt(sock, skb);
    }

    /* If no connected socket and it's a SYN, dispatch to a listening socket */
    if (hdr->syn) {
        sock = get_sock_by_addr(SOCK_TCP, iphdr->dest_ip, ntohs(hdr->be_dest_port), ANY_IP, 0);
        if (sock != NULL && sock->listening) {
            return tcp_handle_syn(sock, skb);
        }
    }

    /* Discard other packets */
    return -1;
}

/* socket() socketcall handler */
int
tcp_socket(net_sock_t *sock)
{
    tcp_sock_t *tcp = malloc(sizeof(tcp_sock_t));
    if (tcp == NULL) {
        debugf("Cannot allocate space for TCP data\n");
        return -1;
    }

    /* TODO */
    sock->private = tcp;
    return 0;
}

/* bind() socketcall handler */
int
tcp_bind(net_sock_t *sock, const sock_addr_t *addr)
{
    /* Can't re-bind connected sockets */
    if (sock->connected) {
        return -1;
    }

    /* Copy address into kernelspace */
    sock_addr_t tmp;
    if (!copy_from_user(&tmp, addr, sizeof(sock_addr_t))) {
        return -1;
    }

    return socket_bind_addr(sock, tmp.ip, tmp.port);
}

/* connect() socketcall handler */
int
tcp_connect(net_sock_t *sock, const sock_addr_t *addr)
{
    /* Cannot connect already-connected or listening sockets */
    if (sock->connected || sock->listening) {
        return -1;
    }

    /* Copy address to kernelspace */
    sock_addr_t tmp;
    if (!copy_from_user(&tmp, addr, sizeof(sock_addr_t))) {
        return -1;
    }

    /* Attempt to connect */
    if (socket_connect_addr(sock, tmp.ip, tmp.port) < 0) {
        return -1;
    }

    /* TODO: Send SYN */
    return 0;
}

/* listen() socketcall handler */
int
tcp_listen(net_sock_t *sock, int backlog)
{
    if (sock->connected) {
        return -1;
    }

    sock->listening = true;
    return 0;
}

/* accept() socketcall handler */
int
tcp_accept(net_sock_t *sock, sock_addr_t *addr)
{
    /* Cannot call accept() on a non-listening socket */
    if (!sock->listening) {
        return -1;
    }

    /* Check if we have any pending connections */
    tcp_sock_t *tcp = sock->private;
    if (list_empty(&tcp->backlog)) {
        return -EAGAIN;
    }

    /* Create a new socket */
    /* TODO */
    return -1;
}

/* recvfrom() socketcall handler */
int
tcp_recvfrom(net_sock_t *sock, void *buf, int nbytes, sock_addr_t *addr)
{
    /* Can't specify an address on a TCP socket */
    if (addr != NULL) {
        return -1;
    }

    /* TODO */
    return -1;
}

/* sendto() socketcall handler */
int
tcp_sendto(net_sock_t *sock, const void *buf, int nbytes, const sock_addr_t *addr)
{
    /* Can't specify an address on a TCP socket */
    if (addr != NULL) {
        return -1;
    }

    /* TODO */
    return -1;
}

/* close() socketcall handler */
int
tcp_close(net_sock_t *sock)
{
    /* TODO */
    return 0;
}
