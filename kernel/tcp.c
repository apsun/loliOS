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
 * If this option is enabled, we randomly drop some packets, simulating
 * real-world network conditions. This is necessary since QEMU's
 * SLIRP is implemented on top of the host OS's TCP sockets, which
 * means data will always arrive in-order.
 */
#define TCP_DEBUG 0
#define TCP_DEBUG_RX_DROP_FREQ 5
#define TCP_DEBUG_TX_DROP_FREQ 5

/* State of a TCP connection */
typedef enum {
    LISTEN,       /* Waiting for SYN */
    SYN_SENT,     /* SYN sent, waiting for SYN-ACK */
    SYN_RECEIVED, /* SYN received, waiting for ACK */
    ESTABLISHED,  /* Three-way handshake complete */
    FIN_WAIT_1,   /* close() */
    FIN_WAIT_2,   /* close() -> ACK received */
    CLOSING,      /* close() -> FIN received */
    TIME_WAIT,    /* close() -> FIN received, ACK received */
    CLOSE_WAIT,   /* FIN received */
    LAST_ACK,     /* FIN received -> close() called */
} tcp_state_t;

/* TCP socket state */
typedef struct {
    /* Back-pointer to the socket object */
    net_sock_t *sock;

    /* Current state of the connection */
    tcp_state_t state;

    /*
     * Linked list of incoming TCP packets. This list is maintained
     * in increasing order of remote sequence number, and may have holes
     * and overlaps.
     */
    list_t inbox;

    /*
     * Linked list of outgoing TCP packets that have not been sent or
     * ACKed yet. This list is maintained in increasing order of local
     * sequence number, and never has holes or overlaps.
     */
    list_t outbox;

    /* Remote sequence number that the application has consumed up to */
    uint32_t read_num;

    /* Remote sequence number of next consecutive packet we expect */
    uint32_t ack_num;

    /* Local sequence number of next packet to be added to the outbox */
    uint32_t seq_num;

    /* Whether we've consumed the SYN/FIN "bytes" */
    bool read_syn : 1;
    bool read_fin : 1;
} tcp_sock_t;

/* Obtains a tcp_sock_t reference from a net_sock_t */
#define tcp_sock(sock) ((tcp_sock_t *)(sock)->private)

/*
 * Since sequence numbers can wrap around, use this macro to
 * determine order.
 */
#define cmp(a, b) ((int)((a) - (b)))

/*
 * Returns the body length of the given TCP packet.
 */
static int
tcp_body_len(skb_t *skb)
{
    tcp_hdr_t *hdr = skb_transport_header(skb);
    ip_hdr_t *iphdr = skb_network_header(skb);
    int tcp_hdr_len = hdr->data_offset * 4;
    if (iphdr == NULL) {
        /* No IP or Ethernet header */
        return skb_len(skb) - tcp_hdr_len;
    } else {
        /* IP, maybe Ethernet headers */
        return ntohs(iphdr->be_total_length) - iphdr->ihl * 4 - tcp_hdr_len;
    }
}

/*
 * Returns the "sequence length" of the given TCP packet.
 * This is usually equal to the body length, except when the
 * packet contains a SYN or FIN, in which case the length
 * is advanced by an additional imaginary byte.
 */
static int
tcp_seq_len(skb_t *skb)
{
    int len = tcp_body_len(skb);
    tcp_hdr_t *hdr = skb_transport_header(skb);
    if (hdr->syn) {
        len++;
    }
    if (hdr->fin) {
        len++;
    }
    return len;
}

/*
 * Whether this socket has received a SYN from the remote peer
 * (and thus, knows their sequence number).
 */
static bool
tcp_received_syn(tcp_sock_t *tcp)
{
    switch (tcp->state) {
    case LISTEN:
    case SYN_SENT:
        return false;
    default:
        return true;
    }
}

/*
 * Returns the local window size of a TCP connection.
 */
static uint16_t
tcp_window_size(tcp_sock_t *tcp)
{
    /* TODO */
    return 8192;
}

/*
 * Converts a TCP state constant to a string representation,
 * for use in debugging.
 */
static const char *
tcp_get_state_str(tcp_state_t state)
{
    static const char *names[] = {
#define ENTRY(x) [x] = #x
        ENTRY(LISTEN),
        ENTRY(SYN_SENT),
        ENTRY(SYN_RECEIVED),
        ENTRY(ESTABLISHED),
        ENTRY(FIN_WAIT_1),
        ENTRY(FIN_WAIT_2),
        ENTRY(CLOSING),
        ENTRY(TIME_WAIT),
        ENTRY(CLOSE_WAIT),
        ENTRY(LAST_ACK),
#undef ENTRY
    };
    return names[state];
}

/*
 * Sets the state of a TCP connection.
 */
static void
tcp_set_state(tcp_sock_t *tcp, tcp_state_t state)
{
    debugf("TCP state %s -> %s\n",
        tcp_get_state_str(tcp->state),
        tcp_get_state_str(state));
    tcp->state = state;
}

/*
 * Allocates and partially initializes a new TCP packet.
 * All fields are set to zero, except the port and data
 * offset fields. This does NOT increment the TCP sequence
 * number.
 */
static skb_t *
tcp_alloc_skb(net_sock_t *sock, size_t body_len)
{
    size_t hdr_len = sizeof(tcp_hdr_t) + sizeof(ip_hdr_t) + sizeof(ethernet_hdr_t);
    skb_t *skb = skb_alloc(hdr_len + body_len);
    if (skb == NULL) {
        return NULL;
    }

    skb_reserve(skb, hdr_len);
    tcp_hdr_t *hdr = skb_push(skb, sizeof(tcp_hdr_t));
    skb_reset_transport_header(skb);
    hdr->be_src_port = htons(sock->local.port);
    hdr->be_dest_port = htons(sock->remote.port);
    hdr->be_seq_num = htonl(0);
    hdr->be_ack_num = htonl(0);
    hdr->ns = 0;
    hdr->reserved = 0;
    hdr->data_offset = sizeof(tcp_hdr_t) / 4;
    hdr->fin = 0;
    hdr->syn = 0;
    hdr->rst = 0;
    hdr->psh = 0;
    hdr->ack = 0;
    hdr->urg = 0;
    hdr->ece = 0;
    hdr->cwr = 0;
    hdr->be_window_size = htons(0);
    hdr->be_checksum = htons(0);
    hdr->be_urg_ptr = 0;
    return skb;
}

/*
 * Sends a TCP packet to the connected remote peer.
 */
static int
tcp_send(net_sock_t *sock, skb_t *skb)
{
    tcp_sock_t *tcp = tcp_sock(sock);
    tcp_hdr_t *hdr = skb_transport_header(skb);

    /*
     * If we know the peer's sequence number, always send an ACK
     * alongside any data we send for the latest in-order segment
     * we've received so far.
     */
    if (tcp_received_syn(tcp)) {
        hdr->ack = 1;
        hdr->be_ack_num = htonl(tcp->ack_num);
    }

    /* Determine next-hop IP address */
    ip_addr_t dest_ip = sock->remote.ip;
    ip_addr_t neigh_ip;
    net_iface_t *iface = net_route(sock->iface, dest_ip, &neigh_ip);
    if (iface == NULL) {
        debugf("Cannot send packet via bound interface\n");
        return -1;
    }

    /* Update window size */
    hdr->be_window_size = htons(tcp_window_size(tcp));

    /* Re-compute checksum */
    hdr->be_checksum = htons(ip_pseudo_checksum(
        skb, iface->ip_addr, dest_ip, IPPROTO_TCP));

    /* If debugging is enabled, randomly drop some packets */
#if TCP_DEBUG
    if (rand() % 100 < TCP_DEBUG_TX_DROP_FREQ) {
        return 0;
    }
#endif

    /* And awaaaaaaay we go! */
    return ip_send(iface, neigh_ip, skb, dest_ip, IPPROTO_TCP);
}

/*
 * Adds an outgoing TCP packet to the outbox queue.
 */
static void
tcp_outbox_insert(tcp_sock_t *tcp, skb_t *skb)
{
    /*
     * Since we add stuff to the outbox in a monotonically
     * increasing order, no need for any list traversal stuff
     * here. Just add it at the end of the queue.
     */
    list_add_tail(&skb_retain(skb)->list, &tcp->outbox);
}

/*
 * Inserts an incoming TCP packet into the specified socket's
 * inbox, so that its data can be read later in recvfrom().
 */
static void
tcp_inbox_insert(tcp_sock_t *tcp, skb_t *skb)
{
    tcp_hdr_t *hdr = skb_transport_header(skb);

    /*
     * Find appropriate place in the inbox queue to insert the packet.
     * Iterate from the tail since most packets probably arrive in
     * the correct order. Algorithm: find the latest position in
     * the list where SEQ(new) > SEQ(entry), and insert the new packet
     * after the existing entry. If it happens that the new packet
     * belongs at the head of the queue, we rely on the implementation
     * of list_for_each_prev to leave pos == head once the loop ends.
     *
     * We do NOT discard retransmissions; this makes the logic a lot
     * simpler. We just handle it as if it was an overlapping segment.
     */
    list_t *pos;
    list_for_each_prev(pos, &tcp->inbox) {
        skb_t *iskb = list_entry(pos, skb_t, list);
        tcp_hdr_t *ihdr = skb_transport_header(iskb);
        if (cmp(ntohl(hdr->be_seq_num), ntohl(ihdr->be_seq_num)) > 0) {
            break;
        }
    }
    list_add(&skb_retain(skb)->list, pos);
}

/*
 * Updates the ack_num field of the TCP connection - that is, the
 * latest in-order segment we've received from the remote peer.
 * This value is injected into any outgoing packet's ACK field.
 */
static void
tcp_update_ack_num(tcp_sock_t *tcp)
{
    /*
     * Example:
     *
     * [ 0 ]    [ 2 ] -> [ 4 ]
     *   |        ^
     *   |        |
     *   +->[ 1 ]-+
     *
     * Say we just received packet SEQ=1. Before this function, ack_num
     * should be 1, since we've previously processed packet SEQ=0.
     * The loop iterations will proceed as follows:
     *
     * first iteration:  cmp(seq=0, ack_num=1) < 0 -> ack_num = 1
     * second iteration: cmp(seq=1, ack_num=1) = 0 -> ack_num = 2
     * third iteration:  cmp(seq=2, ack_num=2) = 0 -> ack_num = 3
     * fourth iteration: cmp(seq=4, ack_num=3) > 0 -> break
     * final state: ack_num == 3
     */
    list_t *pos;
    list_for_each(pos, &tcp->inbox) {
        skb_t *iskb = list_entry(pos, skb_t, list);
        tcp_hdr_t *ihdr = skb_transport_header(iskb);

        /* If seq > ack_num, we have a hole, so stop here */
        if (cmp(ntohl(ihdr->be_seq_num), tcp->ack_num) > 0) {
            break;
        }

        /* Represents latest consecutive sequence number we've seen + 1 */
        uint32_t end = ntohl(ihdr->be_seq_num) + tcp_seq_len(iskb);

        /*
         * Otherwise, we advance the next expected sequence number.
         * Note that some segments may overlap, which is why we take
         * the maximum of the two sequence numbers. For example, if
         * ack_num = 5 and we get a packet containing seq = 4 ~ 7,
         * then we advance ack_num to 8.
         */
        if (cmp(end, tcp->ack_num) > 0) {
            tcp->ack_num = end;
        }
    }
}

/*
 * Creates and sends a new SYN packet to the remote peer.
 */
static int
tcp_send_syn(net_sock_t *sock)
{
    tcp_sock_t *tcp = tcp_sock(sock);

    /* Allocate packet */
    skb_t *skb = tcp_alloc_skb(sock, 0);
    if (skb == NULL) {
        return -1;
    }

    /* Initialize packet */
    tcp_hdr_t *hdr = skb_transport_header(skb);
    hdr->syn = 1;
    hdr->be_seq_num = htonl(tcp->seq_num++);

    /* Enqueue packet in outbox and immediately send */
    tcp_outbox_insert(tcp, skb);
    int ret = tcp_send(sock, skb);
    skb_release(skb);
    return ret;
}

static int
tcp_send_ack(net_sock_t *sock)
{
    tcp_sock_t *tcp = tcp_sock(sock);

    /* Allocate packet */
    skb_t *skb = tcp_alloc_skb(sock, 0);
    if (skb == NULL) {
        return -1;
    }

    tcp_hdr_t *hdr = skb_transport_header(skb);
    hdr->be_seq_num = htonl(tcp->seq_num);

    /* Don't enqueue packet, just directly send empty ACK */
    int ret = tcp_send(sock, skb);
    skb_release(skb);
    return ret;
}

/*
 * Handles an incoming ACK packet. This will purge ACKed
 * packets from the outbox. If duplicate ACKs are detected,
 * this may also result in a retransmission.
 */
static void
tcp_handle_rx_ack(tcp_sock_t *tcp, uint32_t ack_num)
{
    int num_acked = 0;
    list_t *pos, *next;
    list_for_each_safe(pos, next, &tcp->outbox) {
        skb_t *oskb = list_entry(pos, skb_t, list);
        tcp_hdr_t *ohdr = skb_transport_header(oskb);

        /*
         * Since ACK is for the next expected sequence number,
         * it's only useful when SEQ(pkt) + SEQ_LEN(pkt) < ack_num.
         */
        int seq_len = tcp_seq_len(oskb);
        if (cmp(ntohs(ohdr->be_ack_num) + seq_len, ack_num) >= 0) {
            break;
        }

        /* We got an ACK for our SYN! Move to established state. */
        if (ohdr->syn && tcp->state < ESTABLISHED) {
            tcp_set_state(tcp, ESTABLISHED);
        }

        /* No longer need to keep track of this packet! */
        list_del(pos);
        skb_release(oskb);
        num_acked++;
    }

    /* Duplicate ACK received */
    if (num_acked == 0) {
        /* TODO */
    }
}

/*
 * Handles an incoming SYN to a listening socket.
 */
static int
tcp_handle_rx_listening(net_sock_t *sock, skb_t *skb)
{
    /* TODO */
    debugf("tcp_handle_syn()\n");
    return -1;
}

/*
 * Handles an incoming packet to a connected socket.
 */
static int
tcp_handle_rx_connected(net_sock_t *sock, skb_t *skb)
{
    tcp_sock_t *tcp = tcp_sock(sock);
    tcp_hdr_t *hdr = skb_transport_header(skb);

    /* Insert packet into inbox */
    tcp_inbox_insert(tcp, skb);

    /*
     * If we were waiting for a SYN, either this packet contains it,
     * in which case we should start advancing the ACK number, or
     * it doesn't, in which case we delay that until we actually
     * know the initial sequence number.
     */
    if (tcp->state == SYN_SENT) {
        if (hdr->syn) {
            tcp_set_state(tcp, ESTABLISHED);
            tcp->ack_num = ntohl(hdr->be_seq_num);
            tcp->read_num = tcp->ack_num;
        } else {
            return 0;
        }
    }

    /* Update the ack_num */
    tcp_update_ack_num(tcp);

    /* If packet contained an ACK, process that now */
    if (hdr->ack) {
        tcp_handle_rx_ack(tcp, ntohs(hdr->be_ack_num));
    }

    tcp_send_ack(sock);
    return 0;
}

/* Handles reception of a TCP packet */
int
tcp_handle_rx(net_iface_t *iface, skb_t *skb)
{
    /* If debugging is enabled, randomly drop some packets */
#if TCP_DEBUG
    if (rand() % 100 < TCP_DEBUG_RX_DROP_FREQ) {
        return 0;
    }
#endif

    /* Check packet size */
    if (!skb_may_pull(skb, sizeof(tcp_hdr_t))) {
        debugf("TCP packet too small\n");
        return -1;
    }

    /* Pop TCP header */
    ip_hdr_t *iphdr = skb_network_header(skb);
    tcp_hdr_t *hdr = skb_reset_transport_header(skb);
    skb_pull(skb, sizeof(tcp_hdr_t));

    /* Try to dispatch to a connected socket */
    net_sock_t *sock = get_sock_by_addr(
        SOCK_TCP,
        iphdr->dest_ip, ntohs(hdr->be_dest_port),
        iphdr->src_ip, ntohs(hdr->be_src_port));
    if (sock != NULL) {
        return tcp_handle_rx_connected(sock, skb);
    }

    /* If no connected socket and it's a SYN, dispatch to a listening socket */
    if (hdr->syn) {
        sock = get_sock_by_addr(
            SOCK_TCP,
            iphdr->dest_ip, ntohs(hdr->be_dest_port),
            ANY_IP, 0);
        if (sock != NULL && sock->listening) {
            return tcp_handle_rx_listening(sock, skb);
        }
    }

    /* Discard other packets */
    debugf("Packet to unknown socket\n");
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

    tcp->sock = sock;
    sock->private = tcp;

    tcp->state = LISTEN;
    list_init(&tcp->inbox);
    list_init(&tcp->outbox);
    tcp->read_num = 0;
    tcp->ack_num = 0;
    tcp->seq_num = rand();
    tcp->read_syn = 0;
    tcp->read_fin = 0;
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
        debugf("Could not connect socket\n");
        return -1;
    }

    /* Auto-bind socket if not done already */
    if (!sock->bound && socket_bind_addr(sock, ANY_IP, 0) < 0) {
        debugf("Could not auto-bind socket\n");
        return -1;
    }

    tcp_set_state(tcp_sock(sock), SYN_SENT);
    tcp_send_syn(sock);
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
    if (!sock->listening || !sock->bound) {
        return -1;
    }

    /* TODO */
    return -1;
}

/* recvfrom() socketcall handler */
int
tcp_recvfrom(net_sock_t *sock, void *buf, int nbytes, sock_addr_t *addr)
{
    /* Standard error checks */
    if (nbytes < 0 || addr != NULL || !sock->connected) {
        return -1;
    }

    /* Three way handshake not yet complete */
    tcp_sock_t *tcp = tcp_sock(sock);
    if (tcp->state < ESTABLISHED) {
        return -EAGAIN;
    }

    uint8_t *bufp = buf;
    int copied = 0;
    while (!tcp->read_fin && !list_empty(&tcp->inbox)) {
        skb_t *skb = list_first_entry(&tcp->inbox, skb_t, list);
        tcp_hdr_t *hdr = skb_transport_header(skb);

        /*
         * If this packet hasn't been ACKed yet, we must have a hole,
         * so stop here.
         */
        uint32_t pkt_seq_num = ntohl(hdr->be_seq_num);
        if (cmp(pkt_seq_num, tcp->ack_num) >= 0) {
            break;
        }

        /*
         * If we're starting to read a SYN, advance read_num.
         * Discard any packets earlier than the SYN. Also note
         * that SYN takes up one imaginary byte.
         */
        if (hdr->syn && tcp->read_num == pkt_seq_num) {
            tcp->read_num++;
            tcp->read_syn = true;
        } else if (!tcp->read_syn) {
            goto release_pkt;
        }

        /* Where in the packet body we should begin reading */
        int seq_offset = tcp->read_num - pkt_seq_num;
        int byte_offset = hdr->syn ? seq_offset - 1 : seq_offset;
        int bytes_remaining = tcp_body_len(skb) - byte_offset;
        if (bytes_remaining <= 0) {
            goto release_pkt;
        }

        /* Clamp to actual size of buffer */
        int bytes_to_copy = bytes_remaining;
        if (bytes_to_copy > nbytes - copied) {
            bytes_to_copy = nbytes - copied;
        }

        /* Now do the copy, only return -1 if no bytes could be copied */
        uint8_t *body = skb_data(skb);
        uint8_t *start = &body[byte_offset];
        if (!copy_to_user(&bufp[copied], start, bytes_to_copy)) {
            if (copied == 0) {
                return -1;
            } else {
                return copied;
            }
        }
        tcp->read_num += bytes_to_copy;
        copied += bytes_to_copy;

        /*
         * If we didn't copy the entire thing, user buffer must have
         * been too small. Stop here and try again next time.
         */
        if (bytes_to_copy < bytes_remaining) {
            break;
        }

        /*
         * Done with this packet. If it contained a FIN, mark end
         * of stream (note FIN also takes 1 imaginary byte). Remove
         * the packet from the inbox.
         */
release_pkt:
        if (hdr->fin) {
            tcp->read_num++;
            tcp->read_fin = true;
        }
        list_del(&skb->list);
        skb_release(skb);
    }

    /*
     * If we didn't copy anything and we hit the FIN, there's no
     * more data in the stream to read. Otherwise, it just means
     * we didn't get any data yet, so return -EAGAIN.
     */
    if (copied == 0) {
        if (tcp->read_fin) {
            return 0;
        } else {
            return -EAGAIN;
        }
    }

    return copied;
}

/* sendto() socketcall handler */
int
tcp_sendto(net_sock_t *sock, const void *buf, int nbytes, const sock_addr_t *addr)
{
    /* Standard error checks */
    if (nbytes < 0 || addr != NULL || !sock->connected) {
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
