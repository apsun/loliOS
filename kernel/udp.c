#include "udp.h"
#include "lib.h"
#include "debug.h"
#include "paging.h"
#include "net.h"
#include "ip.h"
#include "ethernet.h"

typedef struct {
} udp_pseudohdr_t;

/* UDP socket private data */
typedef struct {
    bool used;
    skb_t *inbox[16];
    int inbox_num;
} udp_sock_t;

/* One private data per socket */
static udp_sock_t udp_socks[36];

/*
 * Computes the UDP checksum. The SKB should contain the
 * UDP header already, with the checksum set to zero. The
 * source IP is the address of the interface that will be
 * used to send the datagram.
 */
static uint16_t
udp_checksum(skb_t *skb, ip_addr_t src_ip, ip_addr_t dest_ip)
{
    ip_pseudo_hdr_t phdr;
    phdr.src_ip = src_ip;
    phdr.dest_ip = dest_ip;
    phdr.zero = 0;
    phdr.protocol = IPPROTO_UDP;
    phdr.be_length = htons(skb_len(skb));
    uint16_t sum = ip_checksum(
        ip_partial_checksum(&phdr, sizeof(phdr)) +
        ip_partial_checksum(skb_data(skb), skb_len(skb)));
    if (sum == 0) {
        sum = 0xffff;
    }
    return sum;
}

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
    udp_hdr_t *hdr = skb_reset_transport_header(skb);
    if (htons(hdr->be_length) != skb_len(skb)) {
        debugf("UDP datagram size mismatch\n");
        return -1;
    }
    skb_pull(skb, sizeof(udp_hdr_t));

    /* Find the corresponding socket */
    ip_addr_t dest_ip = iface->ip_addr;
    uint16_t dest_port = ntohs(hdr->be_dest_port);
    net_sock_t *sock = get_sock_by_addr(SOCK_UDP, dest_ip, dest_port);
    if (sock == NULL || sock->type != SOCK_UDP) {
        debugf("No UDP socket for (IP, port), dropping datagram\n");
        return -1;
    }

    /* Append SKB to socket queue */
    udp_sock_t *udp = sock->private;
    if (udp->inbox_num == array_len(udp->inbox)) {
        debugf("UDP inbox full, dropping datagram\n");
        return -1;
    }
    udp->inbox[udp->inbox_num++] = skb_retain(skb);
    return 0;
}

/* Sends a UDP datagram to the specified IP and port */
int
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
    hdr->be_src_port = htons(sock->port);
    hdr->be_dest_port = htons(port);
    hdr->be_length = htons(skb_len(skb));
    hdr->be_checksum = htons(0);
    hdr->be_checksum = htons(udp_checksum(skb, iface->ip_addr, ip));
    return ip_send(iface, neigh_ip, skb, ip, IPPROTO_UDP);
}

/* Allocates a UDP socket private data object */
static udp_sock_t *
udp_sock_alloc(void)
{
    int i;
    for (i = 0; i < array_len(udp_socks); ++i) {
        udp_sock_t *udp = &udp_socks[i];
        if (!udp->used) {
            udp->used = true;
            udp->inbox_num = 0;
            return udp;
        }
    }
    return NULL;
}

/* socket() socketcall handler */
int
udp_socket(net_sock_t *sock)
{
    udp_sock_t *udp = udp_sock_alloc();
    if (udp == NULL) {
        debugf("Failed to allocate UDP private data\n");
        return -1;
    }
    sock->private = udp;
    return 0;
}

/* bind() socketcall handler */
int
udp_bind(net_sock_t *sock, const sock_addr_t *addr)
{
    /* Copy bind address into kernelspace */
    sock_addr_t local_addr;
    if (!copy_from_user(&local_addr, addr, sizeof(sock_addr_t))) {
        return -1;
    }

    /* Bind socket address */
    if (socket_bind_addr(sock, local_addr.ip, local_addr.port) < 0) {
        debugf("Could not bind address\n");
        return -1;
    }

    return 0;
}

/* recvfrom() socketcall handler */
int
udp_recvfrom(net_sock_t *sock, void *buf, int nbytes, sock_addr_t *addr)
{
    /* Do we have any queued packets? */
    udp_sock_t *udp = sock->private;
    if (udp->inbox_num == 0) {
        return 0;
    }

    /* Get first packet in the inbox queue */
    skb_t *skb = udp->inbox[0];
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
    memmove(&udp->inbox[0], &udp->inbox[1], (udp->inbox_num - 1) * sizeof(skb_t *));
    udp->inbox_num--;
    skb_release(skb);

    return nbytes;
}

/* sendto() socketcall handler */
int
udp_sendto(net_sock_t *sock, const void *buf, int nbytes, const sock_addr_t *addr)
{
    /* Copy destination address from userspace */
    sock_addr_t dest_addr;
    if (!copy_from_user(&dest_addr, addr, sizeof(sock_addr_t))) {
        return -1;
    }

    /* Check port number (uint16_t so only 0 is invalid) */
    if (dest_addr.port == 0) {
        debugf("Invalid destination port\n");
        return -1;
    }

    /* Allocate SKB with enough space for headers */
    skb_t *skb = skb_alloc();
    skb_reserve(skb, sizeof(udp_hdr_t) + sizeof(ip_hdr_t) + sizeof(ethernet_hdr_t));
    if (nbytes > skb_tailroom(skb)) {
        debugf("Datagram body too long\n");
        skb_release(skb);
        return -1;
    }

    /* Copy datagram body from userspace into SKB */
    void *body = skb_put(skb, nbytes);
    if (!copy_from_user(body, buf, nbytes)) {
        skb_release(skb);
        return -1;
    }

    /* udp_send() will prepend the UDP header for us */
    return udp_send(sock, skb, dest_addr.ip, dest_addr.port);
}

/* close() socketcall handler */
int
udp_close(net_sock_t *sock)
{
    udp_sock_t *udp = sock->private;
    udp->used = false;
    return 0;
}
