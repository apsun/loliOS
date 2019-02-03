#include "udp.h"
#include "lib.h"
#include "debug.h"
#include "list.h"
#include "myalloc.h"
#include "syscall.h"
#include "paging.h"
#include "net.h"
#include "ip.h"
#include "ethernet.h"

/* Maximum length of a UDP datagram body */
#define UDP_MAX_LEN 1472

/* UDP-private socket state */
typedef struct {
    /* Simple queue of incoming packets */
    list_t inbox;
} udp_sock_t;

/* Obtains a udp_sock_t reference from a net_sock_t */
#define udp_sock(sock) ((udp_sock_t *)(sock)->private)

/* Handles reception of a UDP datagram */
int
udp_handle_rx(net_iface_t *iface, skb_t *skb)
{
    /* Check packet size */
    if (!skb_may_pull(skb, sizeof(udp_hdr_t))) {
        debugf("UDP datagram too small\n");
        return -1;
    }

    /* Pop UDP header */
    ip_hdr_t *ip_hdr = skb_network_header(skb);
    udp_hdr_t *hdr = skb_reset_transport_header(skb);
    if (ntohs(hdr->be_length) != skb_len(skb)) {
        debugf("UDP datagram size mismatch\n");
        return -1;
    }
    skb_pull(skb, sizeof(udp_hdr_t));

    /* Find the corresponding socket */
    ip_addr_t dest_ip = ip_hdr->dest_ip;
    uint16_t dest_port = ntohs(hdr->be_dest_port);
    net_sock_t *sock = get_sock_by_local_addr(SOCK_UDP, dest_ip, dest_port);
    if (sock == NULL) {
        debugf("No UDP socket for (IP, port), dropping datagram\n");
        return -1;
    }

    /* Append SKB to inbox queue */
    udp_sock_t *udp = udp_sock(sock);
    list_add_tail(&skb_retain(skb)->list, &udp->inbox);
    return 0;
}

/* Sends a UDP datagram to the specified IP and port */
static int
udp_send(net_sock_t *sock, skb_t *skb, ip_addr_t ip, int port)
{
    /* Auto-bind sender address if not already done */
    if (!sock->bound && socket_bind_addr(sock, ANY_IP, 0) < 0) {
        debugf("Could not auto-bind socket\n");
        return -1;
    }

    /* Find out which interface we're going to send this packet on */
    ip_addr_t neigh_ip;
    net_iface_t *iface = net_route(sock->iface, ip, &neigh_ip);
    if (iface == NULL) {
        debugf("Cannot send packet via bound interface\n");
        return -1;
    }

    /* Prepend UDP header */
    udp_hdr_t *hdr = skb_push(skb, sizeof(udp_hdr_t));
    hdr->be_src_port = htons(sock->local.port);
    hdr->be_dest_port = htons(port);
    hdr->be_length = htons(skb_len(skb));
    hdr->be_checksum = htons(0); /* First set to zero to compute checksum */
    hdr->be_checksum = htons(ip_pseudo_checksum(skb, iface->ip_addr, ip, IPPROTO_UDP));
    return ip_send(iface, neigh_ip, skb, ip, IPPROTO_UDP);
}

/*
 * Checks whether the specified packet should be passed to
 * the user when calling recvfrom(). As per the spec, if
 * the socket is connected, only packets from the connected
 * peer will be accepted.
 */
static bool
udp_can_recv(net_sock_t *sock, skb_t *skb)
{
    if (!sock->connected) {
        return true;
    }

    ip_hdr_t *ip_hdr = skb_network_header(skb);
    if (!ip_equals(sock->remote.ip, ip_hdr->src_ip)) {
        return false;
    }

    udp_hdr_t *udp_hdr = skb_transport_header(skb);
    if (sock->remote.port != ntohs(udp_hdr->be_src_port)) {
        return false;
    }

    return true;
}

/* UDP socket constructor */
int
udp_ctor(net_sock_t *sock)
{
    udp_sock_t *udp = malloc(sizeof(udp_sock_t));
    if (udp == NULL) {
        debugf("Cannot allocate space for UDP data\n");
        return -1;
    }
    list_init(&udp->inbox);
    sock->private = udp;
    return 0;
}

/* UDP socket destructor */
void
udp_dtor(net_sock_t *sock)
{
    /* Release all queued packets */
    udp_sock_t *udp = udp_sock(sock);
    list_t *pos, *next;
    list_for_each_safe(pos, next, &udp->inbox) {
        skb_t *skb = list_entry(pos, skb_t, list);
        skb_release(skb);
    }

    /* Free the UDP data */
    free(udp);
}

/*
 * bind() socketcall handler. Sets the local endpoint
 * address of the socket.
 */
int
udp_bind(net_sock_t *sock, const sock_addr_t *addr)
{
    /* Copy address into kernelspace */
    sock_addr_t tmp;
    if (!copy_from_user(&tmp, addr, sizeof(sock_addr_t))) {
        return -1;
    }

    return socket_bind_addr(sock, tmp.ip, tmp.port);
}

/*
 * connect() socketcall handler. This sets the default
 * address to send datagrams to, and also causes incoming
 * datagrams not from the given address to be discarded.
 */
int
udp_connect(net_sock_t *sock, const sock_addr_t *addr)
{
    /* Copy address to kernelspace */
    sock_addr_t tmp;
    if (!copy_from_user(&tmp, addr, sizeof(sock_addr_t))) {
        return -1;
    }

    return socket_connect_addr(sock, tmp.ip, tmp.port);
}

/*
 * recvfrom() socketcall handler. Reads a single datagram
 * from the socket. The sender's address will be copied to
 * addr if it is not null.
 */
int
udp_recvfrom(net_sock_t *sock, void *buf, int nbytes, sock_addr_t *addr)
{
    /* Can only receive packets after bind() */
    if (!sock->bound) {
        debugf("recvfrom() on unbound socket\n");
        return -1;
    }

    /* Find a packet that we can accept */
    udp_sock_t *udp = udp_sock(sock);
    skb_t *skb;
    while (1) {
        if (list_empty(&udp->inbox)) {
            return -EAGAIN;
        }

        skb = list_first_entry(&udp->inbox, skb_t, list);
        if (udp_can_recv(sock, skb)) {
            break;
        }

        list_del(&skb->list);
        skb_release(skb);
    }

    int len = skb_len(skb);
    if (nbytes > len) {
        nbytes = len;
    }

    /* If user asked for the src addr, copy it from the headers */
    if (addr != NULL) {
        sock_addr_t src_addr;
        ip_hdr_t *ip_hdr = skb_network_header(skb);
        udp_hdr_t *udp_hdr = skb_transport_header(skb);
        src_addr.ip = ip_hdr->src_ip;
        src_addr.port = ntohs(udp_hdr->be_src_port);
        if (!copy_to_user(addr, &src_addr, sizeof(sock_addr_t))) {
            return -1;
        }
    }

    /* Copy packet to userspace */
    if (!copy_to_user(buf, skb_data(skb), nbytes)) {
        return -1;
    }

    /* Shift remaining packets up, free SKB */
    list_del(&skb->list);
    skb_release(skb);
    return nbytes;
}

/*
 * sendto() socketcall handler. Sends a single datagram
 * to the specified remote address. If addr is null, it
 * will be sent to the connected address if previously
 * set by connect().
 */
int
udp_sendto(net_sock_t *sock, const void *buf, int nbytes, const sock_addr_t *addr)
{
    /* If addr is not null, override connected address */
    sock_addr_t dest_addr;
    if (addr == NULL && sock->connected) {
        dest_addr = sock->remote;
    } else if (!copy_from_user(&dest_addr, addr, sizeof(sock_addr_t))) {
        return -1;
    }

    /* Check port number (uint16_t so only 0 is invalid) */
    if (dest_addr.port == 0) {
        debugf("Invalid destination port\n");
        return -1;
    }

    /* Validate datagram length */
    if (nbytes < 0 || nbytes > UDP_MAX_LEN) {
        debugf("Datagram body too long\n");
        return -1;
    }

    /* Allocate a new SKB */
    int hdr_len = sizeof(udp_hdr_t) + sizeof(ip_hdr_t) + sizeof(ethernet_hdr_t);
    skb_t *skb = skb_alloc(nbytes + hdr_len);
    if (skb == NULL) {
        debugf("Failed to allocate new SKB\n");
        return -1;
    }

    /* Reserve space for headers */
    skb_reserve(skb, hdr_len);

    /* Copy datagram body from userspace into SKB */
    void *body = skb_put(skb, nbytes);
    if (!copy_from_user(body, buf, nbytes)) {
        skb_release(skb);
        return -1;
    }

    /* udp_send() will prepend the UDP header for us */
    int ret = udp_send(sock, skb, dest_addr.ip, dest_addr.port);
    skb_release(skb);
    if (ret < 0) {
        return ret;
    } else {
        return nbytes;
    }
}
