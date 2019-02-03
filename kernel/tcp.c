/*
 * Note: This implementation of TCP is not standards-compliant.
 * It does not fully implement the following features:
 *
 * - URG flag
 * - TCP options (window scale, etc)
 * - Congestion/flow control
 * - Delayed ACK
 */
#include "tcp.h"
#include "lib.h"
#include "debug.h"
#include "list.h"
#include "file.h"
#include "timer.h"
#include "syscall.h"
#include "myalloc.h"
#include "socket.h"
#include "paging.h"
#include "net.h"
#include "ip.h"
#include "ethernet.h"

/*
 * Enable for verbose TCP logging. Warning: very verbose.
 */
#define TCP_DEBUG_PRINT 0
#if TCP_DEBUG_PRINT
    #define tcp_debugf(...) debugf(__VA_ARGS__)
#else
    #define tcp_debugf(...) ((void)0)
#endif

/*
 * If this option is enabled, we randomly drop some packets, simulating
 * real-world network conditions. This is necessary since QEMU's
 * SLIRP is implemented on top of the host OS's TCP sockets, which
 * means data will always arrive in-order.
 */
#define TCP_DEBUG_DROP 0
#define TCP_DEBUG_RX_DROP_FREQ 5
#define TCP_DEBUG_TX_DROP_FREQ 5

/* Maximum length of the TCP body */
#define TCP_MAX_LEN 1460

/*
 * Time to wait in the TIME_WAIT and FIN_WAIT_2 states before
 * releasing the socket.
 */
#define TCP_FIN_TIMEOUT (60 * TIMER_HZ)

/*
 * Maximum number of times to attempt retransmitting a packet
 * before killing the connection.
 */
#define TCP_MAX_RETRANSMISSIONS 3

/* Allowed RTO range for retransmission timer */
#define TCP_MIN_RTO (1 * TIMER_HZ)
#define TCP_MAX_RTO (120 * TIMER_HZ)
#define TCP_DEFAULT_RTO (3 * TIMER_HZ)

/* Starting receive window size */
#define TCP_RWND_SIZE 8192

/* State of a TCP connection */
typedef enum {
    LISTEN       = (1 << 0),  /* Waiting for SYN */
    SYN_SENT     = (1 << 1),  /* SYN sent, waiting for SYN-ACK */
    SYN_RECEIVED = (1 << 2),  /* SYN received, waiting for ACK */
    ESTABLISHED  = (1 << 3),  /* Three-way handshake complete */
    FIN_WAIT_1   = (1 << 4),  /* close() */
    FIN_WAIT_2   = (1 << 5),  /* close() -> ACK received */
    CLOSING      = (1 << 6),  /* close() -> FIN received */
    TIME_WAIT    = (1 << 7),  /* close() -> FIN received, ACK received */
    CLOSE_WAIT   = (1 << 8),  /* FIN received */
    LAST_ACK     = (1 << 9),  /* FIN received -> close() called */
    CLOSED       = (1 << 10), /* Used when the connection is closed but file is still open */
} tcp_state_t;

/* TCP socket state */
typedef struct {
    /* Back-pointer to the socket object */
    net_sock_t *sock;

    /* Current state of the connection */
    tcp_state_t state;

    /*
     * This is a weird field used for two purposes: if this is a listening
     * socket, this holds the head of the backlog list. If this is a
     * connected socket, this holds our node in the listening socket's
     * backlog list. For a connected socket that has already been accepted,
     * this field is unused.
     */
    list_t backlog;

    /*
     * Linked list of incoming TCP packets. This list is maintained
     * in increasing order of remote sequence number, and may have holes
     * and overlaps.
     */
    list_t inbox;

    /*
     * Linked list of outgoing TCP packets that have not been sent or
     * ACKed yet. This list is maintained in increasing order of local
     * sequence number, and never has holes or overlaps. The element type
     * is tcp_pkt_t, not skb_t!
     */
    list_t outbox;

    /*
     * Timer for the TIME_WAIT and FIN_WAIT_2 states. When this timer expires,
     * the socket is released.
     */
    timer_t fin_timer;

    /*
     * Receive window size of the socket. This is used to limit the incoming
     * packet rate, so we don't spend 100% of our time handling packets.
     * This value may be negative to indicate that our inbox is fuller than
     * normal; when reading this value it should be treated as zero.
     */
    int rwnd_size;

    /* Remote sequence number that the application has consumed up to */
    uint32_t read_num;

    /* Remote sequence number of next consecutive packet we expect */
    uint32_t ack_num;

    /* Local sequence number of next packet to be added to the outbox */
    uint32_t seq_num;

    /* Earliest local sequence number that has not been acknowledged yet */
    uint32_t unack_num;

    /* Duplicate ACK counter for fast retransmission */
    uint8_t num_duplicate_acks : 2;

    /* Whether the connection has been reset and cannot be read from */
    bool reset : 1;

    /* RTT measurement for timeouts */
    int estimated_rtt;
    int variance_rtt;
} tcp_sock_t;

/*
 * Packet in the TCP outbox. Contains transmission info used to perform
 * retransmission of lost packets.
 */
typedef struct {
    list_t list;
    tcp_sock_t *tcp;
    skb_t *skb;

    /*
     * Timer used to retransmit this packet if it ever gets lost.
     */
    timer_t retransmit_timer;

    /*
     * Number of times we've transmitted this packet. Used for three
     * purposes: (1) queueing packets before the 3-way handshake is
     * complete, so we know which packets to transmit upon completion;
     * (2) limit maximum number of retransmissions; (3) apply Karn's
     * algorithm (only update RTT if no retransmission, and double
     * timeout at every retransmission).
     */
    int num_transmissions;

    /*
     * Time at which we last transmitted this packet, used to update
     * the RTT when we receive the ACK for this packet.
     */
    int transmit_time;
} tcp_pkt_t;

/* Converts between tcp_sock_t and net_sock_t */
#define tcp_sock(sock) ((tcp_sock_t *)(sock)->private)
#define net_sock(tcp) ((tcp)->sock)

/*
 * Since sequence numbers can wrap around, use this macro to
 * determine order.
 */
#define cmp(a, b) ((int)((a) - (b)))
#define ack(hdr) (ntohl((hdr)->be_ack_num))
#define seq(hdr) (ntohl((hdr)->be_seq_num))

/* Forward declaration */
static void tcp_on_retransmit_timeout(timer_t *timer);

/*
 * Generates a random 32-bit integer. Required since our libc
 * rand() only generates a 15 bit value.
 */
static uint32_t
tcp_rand32(void)
{
    uint32_t ret = 0;
    ret |= (rand() & 0xff) << 0;
    ret |= (rand() & 0xff) << 8;
    ret |= (rand() & 0xff) << 16;
    ret |= (rand() & 0xff) << 24;
    return ret;
}

/*
 * Returns the body length of the given TCP packet.
 */
static int
tcp_body_len(skb_t *skb)
{
    tcp_hdr_t *hdr = skb_transport_header(skb);
    int tcp_hdr_len = hdr->data_offset * 4;

    /*
     * We need to handle both outgoing and incoming packets.
     * Incoming packets have an IP header and maybe an Ethernet
     * header; outgoing packets only have a TCP header.
     */
    ip_hdr_t *iphdr = skb_network_header(skb);
    if (iphdr == NULL) {
        /* No IP or Ethernet header */
        return skb_len(skb) - tcp_hdr_len;
    } else {
        /* IP, maybe Ethernet headers */
        return ntohs(iphdr->be_total_length) - iphdr->ihl * 4 - tcp_hdr_len;
    }
}

/*
 * Returns the "segment length" of the given TCP packet.
 * This is usually equal to the body length, except when the
 * packet contains a SYN and/or FIN, in which case the length
 * is advanced by an additional imaginary byte for each.
 */
static int
tcp_seg_len(skb_t *skb)
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
 * Prints the control information of a packet using tcp_debugf().
 */
static void
tcp_dump_pkt(const char *prefix, skb_t *skb)
{
    tcp_hdr_t *hdr __unused = skb_transport_header(skb);
    tcp_debugf("%s: SEQ=%u, LEN=%u, ACK=%u, CTL=%s%s%s%s%s\b\n",
        prefix, seq(hdr), tcp_seg_len(skb), ack(hdr),
        (hdr->fin) ? "FIN+" : "",
        (hdr->syn) ? "SYN+" : "",
        (hdr->rst) ? "RST+" : "",
        (hdr->ack) ? "ACK+" : "",
        (hdr->fin | hdr->syn | hdr->rst | hdr->ack) ? "" : "(none)+");
}

/*
 * Converts a TCP state constant to a string representation,
 * for use in debugging.
 */
__used static const char *
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
        ENTRY(CLOSED),
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
    tcp_debugf("TCP state (0x%08x) %s -> %s\n",
        tcp,
        tcp_get_state_str(tcp->state),
        tcp_get_state_str(state));
    tcp->state = state;
}

/*
 * Returns whether the TCP connection is in one of
 * the specified states.
 */
static bool
tcp_in_state(tcp_sock_t *tcp, tcp_state_t states)
{
    return (tcp->state & states) != 0;
}

/*
 * Returns whether the packet contains a "valid" ACK.
 * Validity is defined as "we've actually sent this
 * sequence number before". This may return incorrect
 * results if the sequence number is so large that it
 * overflows.
 */
static bool
tcp_is_ack_valid(tcp_sock_t *tcp, tcp_hdr_t *hdr)
{
    uint32_t ack_num = ack(hdr);
    return cmp(ack_num, tcp->seq_num) <= 0;
}

/*
 * Returns whether the packet contains a "current" ACK.
 * In the TCP RFC, this is referred to as an "acceptable"
 * ACK - that is, it is in our receive window. Note:
 * this does NOT check that the ACK is valid!
 */
static bool
tcp_is_ack_current(tcp_sock_t *tcp, tcp_hdr_t *hdr)
{
    uint32_t ack_num = ack(hdr);
    return cmp(ack_num, tcp->unack_num) >= 0;
}

/*
 * Returns the local receive window size of a TCP connection.
 */
static uint16_t
tcp_rwnd_size(tcp_sock_t *tcp)
{
    if (tcp->rwnd_size < 0) {
        return 0;
    }
    return tcp->rwnd_size;
}

/*
 * Returns whether the packet is within our receive window,
 * that is, some portion of it lies between our next expected
 * sequence number and the maximum expected sequence number.
 */
static bool
tcp_in_rwnd(tcp_sock_t *tcp, skb_t *skb)
{
    tcp_hdr_t *hdr = skb_transport_header(skb);
    uint32_t seg_len = tcp_seg_len(skb);
    uint16_t rwnd_size = tcp_rwnd_size(tcp);
    uint32_t seq_num = seq(hdr);
    uint32_t ack_num = tcp->ack_num;

    if (rwnd_size == 0) {
        return seg_len == 0 && cmp(seq_num, ack_num) == 0;
    } else {
        return (
            cmp(seq_num, ack_num) >= 0
            && cmp(seq_num, ack_num + rwnd_size) < 0)
        || (
            seg_len > 0
            && cmp(seq_num + seg_len - 1, ack_num) >= 0
            && cmp(seq_num + seg_len - 1, ack_num + rwnd_size) < 0);
    }
}

/*
 * Increments the TCP socket reference count.
 */
static void
tcp_acquire(tcp_sock_t *tcp)
{
    tcp_debugf("acquire(0x%08x)\n", tcp);
    socket_obj_retain(net_sock(tcp));
}

/*
 * Decrements the TCP socket reference count.
 * This may free the socket if the reference count
 * reaches zero, so this must be called after all
 * uses of the socket.
 */
static void
tcp_release(tcp_sock_t *tcp)
{
    tcp_debugf("release(0x%08x)\n", tcp);
    socket_obj_release(net_sock(tcp));
}

/*
 * Allocates and partially initializes a new TCP packet.
 * The caller must set the src/dest ports and the seq
 * number, along with any flags, before sending the packet.
 */
static skb_t *
tcp_alloc_skb(size_t body_len)
{
    size_t hdr_len = sizeof(tcp_hdr_t) + sizeof(ip_hdr_t) + sizeof(ethernet_hdr_t);
    skb_t *skb = skb_alloc(hdr_len + body_len);
    if (skb == NULL) {
        return NULL;
    }

    skb_reserve(skb, hdr_len);
    tcp_hdr_t *hdr = skb_push(skb, sizeof(tcp_hdr_t));
    skb_reset_transport_header(skb);
    hdr->be_src_port = htons(0);
    hdr->be_dest_port = htons(0);
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
    hdr->be_urg_ptr = htons(0);
    return skb;
}

/*
 * Sends a TCP packet to the specified destination.
 * This does not perform auto-ACK or auto-rwnd;
 * use tcp_send() for that. This will still perform
 * checksum re-calculation.
 */
static int
tcp_send_raw(net_iface_t *iface, ip_addr_t dest_ip, skb_t *skb)
{
    /* Determine next-hop IP address */
    ip_addr_t neigh_ip;
    iface = net_route(iface, dest_ip, &neigh_ip);
    if (iface == NULL) {
        return -1;
    }

    /* Re-compute checksum */
    tcp_hdr_t *hdr = skb_transport_header(skb);
    hdr->be_checksum = htons(0);
    hdr->be_checksum = htons(ip_pseudo_checksum(
        skb, iface->ip_addr, dest_ip, IPPROTO_TCP));

    /* If debugging is enabled, randomly drop some packets */
#if TCP_DEBUG_DROP
    if (rand() % 100 < TCP_DEBUG_TX_DROP_FREQ) {
        tcp_dump_pkt("send (dropped)", skb);
        return 0;
    }
#endif

    /* Dump packet contents */
    tcp_dump_pkt("send", skb);

    /* And awaaaaaay we go! */
    return ip_send(iface, neigh_ip, skb, dest_ip, IPPROTO_TCP);
}

/*
 * Sends a TCP packet to the connected remote peer.
 * This performs auto-ACK and auto-rwnd calculation.
 */
static int
tcp_send(tcp_sock_t *tcp, skb_t *skb)
{
    net_sock_t *sock = net_sock(tcp);
    tcp_hdr_t *hdr = skb_transport_header(skb);

    /*
     * If we know the peer's sequence number, always send an ACK
     * alongside any data we send for the latest in-order segment
     * we've received so far.
     */
    if (!tcp_in_state(tcp, SYN_SENT)) {
        hdr->ack = 1;
        hdr->be_ack_num = htonl(tcp->ack_num);
    }

    /* Update window size */
    hdr->be_window_size = htons(tcp_rwnd_size(tcp));

    return tcp_send_raw(sock->iface, sock->remote.ip, skb);
}

/*
 * Callback for when we finally time out. Closes and releases
 * the socket.
 */
static void
tcp_on_fin_timeout(timer_t *timer)
{
    tcp_sock_t *tcp = timer_entry(timer, tcp_sock_t, fin_timer);
    if (!tcp_in_state(tcp, CLOSED)) {
        tcp_debugf("FIN timeout reached, closing\n");
        tcp_set_state(tcp, CLOSED);
        tcp_release(tcp);
    }
}

/*
 * (Re)starts the FIN timeout timer.
 */
static void
tcp_start_fin_timeout(tcp_sock_t *tcp)
{
    timer_setup(&tcp->fin_timer, TCP_FIN_TIMEOUT, tcp_on_fin_timeout);
}

/*
 * Updates the socket RTT statistics with the
 * given sampled RTT time. This time is used to
 * compute the retransmit timeout.
 */
static void
tcp_update_rtt(tcp_sock_t *tcp, int sample_rtt)
{
    /* On the first update, estimated = sample; deviation = sample / 2 */
    if (tcp->estimated_rtt < 0) {
        tcp->estimated_rtt = sample_rtt;
        tcp->variance_rtt = sample_rtt / 2;
    }

    /* Jacobson's algorithm */
    int error = sample_rtt - tcp->estimated_rtt;
    if (error < 0) {
        error = -error;
    }
    tcp->estimated_rtt = 7 * tcp->estimated_rtt / 8 + sample_rtt / 8;
    tcp->variance_rtt = 3 * tcp->variance_rtt / 4 + error / 4;
}

/*
 * Computes the retransmit timeout for the given socket
 * based on the RTT statistics.
 */
static int
tcp_get_retransmit_timeout(tcp_sock_t *tcp)
{
    /* Default RTO if no data collected */
    if (tcp->estimated_rtt < 0) {
        return TCP_DEFAULT_RTO;
    }

    /* RTO = EstRTT + 4*VarRTT, clamped to [MIN_RTO, MAX_RTO] range */
    int rto = tcp->estimated_rtt + 4 * tcp->variance_rtt;
    if (rto < TCP_MIN_RTO) {
        rto = TCP_MIN_RTO;
    } else if (rto > TCP_MAX_RTO) {
        rto = TCP_MAX_RTO;
    }
    return rto;
}

/*
 * Transmits a packet that was already in the outbox.
 */
static int
tcp_outbox_transmit(tcp_sock_t *tcp, tcp_pkt_t *pkt)
{
    /*
     * Start the retransmit timer. Scale the timeout by
     * 2^(number of times we've transmitted this packet - 1),
     * which is part of Karn's algorithm.
     */
    int timeout = tcp_get_retransmit_timeout(tcp) << pkt->num_transmissions;
    timer_setup(&pkt->retransmit_timer, timeout, tcp_on_retransmit_timeout);

    /* Update packet info and send the segment */
    pkt->transmit_time = timer_now();
    pkt->num_transmissions++;
    return tcp_send(tcp, pkt->skb);
}

/*
 * Callback for when a packet transmission times out. If we
 * still have some attempts remaining, retransmit the packet;
 * otherwise give up and kill the connection.
 */
static void
tcp_on_retransmit_timeout(timer_t *timer)
{
    tcp_pkt_t *pkt = timer_entry(timer, tcp_pkt_t, retransmit_timer);
    tcp_sock_t *tcp = pkt->tcp;
    if (tcp_in_state(tcp, CLOSED)) {
        return;
    }

    /* If we've tried too many times, give up and kill the connection */
    if (pkt->num_transmissions > TCP_MAX_RETRANSMISSIONS) {
        tcp_debugf("Too many retransmissions, giving up\n");
        tcp->reset = true;
        tcp_set_state(tcp, CLOSED);
        tcp_release(tcp);
        return;
    }

    /* Finally retransmit the packet (this will start the re-tx timer) */
    tcp_outbox_transmit(tcp, pkt);
}

/*
 * Adds a packet to the TCP outbox queue. This does NOT
 * transmit the packet. Returns the packet that can be
 * passed to tcp_outbox_transmit, or NULL if no memory
 * is available to allocate the packet data.
 */
static tcp_pkt_t *
tcp_outbox_insert(tcp_sock_t *tcp, skb_t *skb)
{
    tcp_pkt_t *pkt = malloc(sizeof(tcp_pkt_t));
    if (pkt == NULL) {
        return NULL;
    }

    pkt->tcp = tcp;
    pkt->skb = skb_retain(skb);
    pkt->num_transmissions = 0;
    pkt->transmit_time = timer_now();
    timer_init(&pkt->retransmit_timer);
    list_add_tail(&pkt->list, &tcp->outbox);
    return pkt;
}

/*
 * Removes a packet from the TCP inbox. This also adjusts the
 * receive window accordingly.
 */
static void
tcp_inbox_remove(tcp_sock_t *tcp, skb_t *skb)
{
    tcp->rwnd_size += tcp_seg_len(skb);
    list_del(&skb->list);
}

/*
 * Used to drain the inbox if we know the user will never be
 * able to read its contents. This is used to free memory and
 * ensure the rwnd doesn't stay at zero forever.
 */
static void
tcp_inbox_drain(tcp_sock_t *tcp)
{
    while (!list_empty(&tcp->inbox)) {
        skb_t *skb = list_first_entry(&tcp->inbox, skb_t, list);
        tcp_hdr_t *hdr = skb_transport_header(skb);

        /*
         * If this packet hasn't been ACKed yet, we must have a hole,
         * so stop here.
         */
        uint32_t pkt_seq_num = seq(hdr);
        if (cmp(pkt_seq_num, tcp->ack_num) > 0) {
            break;
        }

        tcp_inbox_remove(tcp, skb);
        skb_release(skb);
    }
}

/*
 * Inserts an incoming TCP packet into the specified socket's
 * inbox, so that its data can be read later in recvfrom().
 * This will advance the ack_num, and also process FIN flags
 * once they are reached in-order.
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
        if (cmp(seq(hdr), seq(ihdr)) > 0) {
            break;
        }
    }
    list_add(&skb_retain(skb)->list, pos);

    /* Adjust the receive window */
    tcp->rwnd_size -= tcp_seg_len(skb);

    /*
     * If we get more packets while the FIN timer is active, restart
     * the timeout. In TIME_WAIT state, this means that the remote
     * peer might not have received our ACK for their FIN; in FIN_WAIT_2,
     * this indicates that the remote peer has more packets to send.
     */
    if (tcp_in_state(tcp, TIME_WAIT | FIN_WAIT_2)) {
        tcp_start_fin_timeout(tcp);
    }

    /*
     * Next, update our ACK number, which is what we will send
     * to the remote host in any packets containing an ACK. Basically,
     * just process the packets in order until we find a gap.
     */
    list_t *next;
    list_for_each_safe(pos, next, &tcp->inbox) {
        skb_t *iskb = list_entry(pos, skb_t, list);
        tcp_hdr_t *ihdr = skb_transport_header(iskb);

        /* If seq > ack_num, we have a hole, so stop here */
        if (cmp(seq(ihdr), tcp->ack_num) > 0) {
            break;
        }

        /*
         * Check if we've already seen this segment before. Note
         * that some segments may overlap, so we check the ending
         * sequence number of the packet.
         */
        uint32_t end = seq(ihdr) + tcp_seg_len(iskb);
        if (cmp(end, tcp->ack_num) <= 0) {
            continue;
        }

        /* Discard any packets after a FIN */
        if (tcp_in_state(tcp, CLOSING | TIME_WAIT | CLOSE_WAIT | LAST_ACK | CLOSED)) {
            tcp_inbox_remove(tcp, iskb);
            skb_release(iskb);
            continue;
        }

        /* Looks good, advance the ACK number */
        tcp->ack_num = end;

        /* Reached a FIN for the first time */
        if (hdr->fin) {
            if (tcp_in_state(tcp, SYN_RECEIVED | ESTABLISHED)) {
                tcp_set_state(tcp, CLOSE_WAIT);
            } else if (tcp_in_state(tcp, FIN_WAIT_1)) {
                /*
                 * Since we process the ACK before the inbox, we would
                 * already have been in the FIN_WAIT_2 state if we got
                 * a ACK for our FIN.
                 */
                tcp_set_state(tcp, CLOSING);
            } else if (tcp_in_state(tcp, FIN_WAIT_2)) {
                tcp_set_state(tcp, TIME_WAIT);
                tcp_start_fin_timeout(tcp);
            }
        }
    }
}

/*
 * Creates and sends a new SYN packet to the remote peer.
 */
static int
tcp_send_syn(tcp_sock_t *tcp)
{
    net_sock_t *sock = net_sock(tcp);

    /* Allocate packet */
    skb_t *skb = tcp_alloc_skb(0);
    if (skb == NULL) {
        return -1;
    }

    /* Initialize packet */
    tcp_hdr_t *hdr = skb_transport_header(skb);
    hdr->be_src_port = htons(sock->local.port);
    hdr->be_dest_port = htons(sock->remote.port);
    hdr->be_seq_num = htonl(tcp->seq_num++);
    hdr->syn = 1;

    /* Enqueue packet in outbox and immediately send */
    tcp_pkt_t *pkt = tcp_outbox_insert(tcp, skb);
    if (pkt == NULL) {
        skb_release(skb);
        return -1;
    }
    tcp_outbox_transmit(tcp, pkt);
    skb_release(skb);
    return 0;
}

/*
 * Creates and sends a new SYN packet to the remote peer.
 */
static int
tcp_send_fin(tcp_sock_t *tcp)
{
    net_sock_t *sock = net_sock(tcp);

    /* Allocate packet */
    skb_t *skb = tcp_alloc_skb(0);
    if (skb == NULL) {
        return -1;
    }

    /* Initialize packet */
    tcp_hdr_t *hdr = skb_transport_header(skb);
    hdr->be_src_port = htons(sock->local.port);
    hdr->be_dest_port = htons(sock->remote.port);
    hdr->be_seq_num = htonl(tcp->seq_num++);
    hdr->fin = 1;

    /* Enqueue packet in outbox and immediately send */
    tcp_pkt_t *pkt = tcp_outbox_insert(tcp, skb);
    if (pkt == NULL) {
        skb_release(skb);
        return -1;
    }
    tcp_outbox_transmit(tcp, pkt);
    skb_release(skb);
    return 0;
}

/*
 * Creates and sends a new ACK packet to the remote peer.
 */
static int
tcp_send_ack(tcp_sock_t *tcp)
{
    net_sock_t *sock = net_sock(tcp);

    /* Allocate packet */
    skb_t *skb = tcp_alloc_skb(0);
    if (skb == NULL) {
        return -1;
    }

    /* Initialize packet */
    tcp_hdr_t *hdr = skb_transport_header(skb);
    hdr->be_src_port = htons(sock->local.port);
    hdr->be_dest_port = htons(sock->remote.port);
    hdr->be_seq_num = htonl(tcp->seq_num);

    /* Don't enqueue packet, just directly send empty ACK */
    tcp_send(tcp, skb);
    skb_release(skb);
    return 0;
}

/*
 * Replies to an incoming packet with a RST packet.
 * Since we won't always have a TCP socket, this takes
 * the interface we received the packet on instead,
 * and infers the rest of the arguments from the
 * original packet.
 */
static int
tcp_reply_rst(net_iface_t *iface, skb_t *orig_skb)
{
    /* Allocate packet */
    skb_t *skb = tcp_alloc_skb(0);
    if (skb == NULL) {
        return -1;
    }

    /*
     * As per TCP spec, if the original packet contained
     * an ACK, we reply with SEQ=SEG.ACK, CTL=RST. Otherwise,
     * we reply with SEQ=0, ACK=SEG.SEQ+SEG.LEN, CTL=RST+ACK.
     */
    tcp_hdr_t *hdr = skb_transport_header(skb);
    tcp_hdr_t *orig_hdr = skb_transport_header(orig_skb);
    hdr->be_src_port = orig_hdr->be_dest_port;
    hdr->be_dest_port = orig_hdr->be_src_port;
    hdr->rst = 1;
    if (orig_hdr->ack) {
        hdr->be_seq_num = orig_hdr->be_ack_num;
    } else {
        hdr->ack = 1;
        hdr->be_ack_num = htonl(seq(orig_hdr) + tcp_seg_len(orig_skb));
    }

    ip_hdr_t *orig_iphdr = skb_network_header(orig_skb);
    tcp_send_raw(iface, orig_iphdr->src_ip, skb);
    skb_release(skb);
    return 0;
}

/*
 * Closes the write end of a full-duplex socket.
 */
static void
tcp_close_write(tcp_sock_t *tcp)
{
    if (tcp_in_state(tcp, LISTEN | SYN_SENT)) {
        tcp_set_state(tcp, CLOSED);
        tcp_release(tcp);
    } else if (tcp_in_state(tcp, SYN_RECEIVED | ESTABLISHED)) {
        tcp_set_state(tcp, FIN_WAIT_1);
        if (tcp_send_fin(tcp) < 0) {
            tcp->reset = true;
            tcp_set_state(tcp, CLOSED);
            tcp_release(tcp);
        }
    } else if (tcp_in_state(tcp, CLOSE_WAIT)) {
        tcp_set_state(tcp, LAST_ACK);
        if (tcp_send_fin(tcp) < 0) {
            /*
             * Since we were in close-wait state, we know there's no
             * more data from the remote peer, so this is fine. No
             * need to set the reset flag.
             */
            tcp_set_state(tcp, CLOSED);
            tcp_release(tcp);
        }
    }
}

/*
 * Handles an incoming ACK packet. This will purge ACKed
 * packets from the outbox. If duplicate ACKs are detected,
 * this may also result in a retransmission. Returns the
 * number of packets that were ACKed.
 */
static int
tcp_handle_rx_ack(tcp_sock_t *tcp, uint32_t ack_num)
{
    int num_acked = 0;
    list_t *pos, *next;
    list_for_each_safe(pos, next, &tcp->outbox) {
        tcp_pkt_t *opkt = list_entry(pos, tcp_pkt_t, list);
        skb_t *oskb = opkt->skb;
        tcp_hdr_t *ohdr = skb_transport_header(oskb);

        /*
         * Since ACK is for the next expected sequence number,
         * it's only useful when SEQ(pkt) + SEG_LEN(pkt) <= ack_num.
         * If this is an ACK for this specific packet, update the RTT.
         */
        int d = cmp(seq(ohdr) + tcp_seg_len(oskb), ack_num);
        if (d > 0) {
            break;
        } else if (d == 0) {
            /*
             * Karn's algorithm: Only update RTT on packets that have
             * been transmitted once to avoid ambiguous results on
             * retransmitted packets.
             */
            if (opkt->num_transmissions == 1) {
                tcp_update_rtt(tcp, timer_now() - opkt->transmit_time);
            }
        }

        /*
         * We got an ACK for our SYN. Note that this function is
         * only called in the SYN_SENT state if we just received
         * a SYN, so it is correct to move from SYN_SENT to ESTABLISHED
         * here.
         */
        if (ohdr->syn && tcp_in_state(tcp, SYN_SENT | SYN_RECEIVED)) {
            tcp_set_state(tcp, ESTABLISHED);

            /*
             * Also transmit any packets that were waiting for
             * the 3-way handshake to be sent. This will
             * not retransmit the SYN, since that must have had
             * num_transmissions > 0.
             */
            list_t *txpos;
            list_for_each(txpos, &tcp->outbox) {
                tcp_pkt_t *txpkt = list_entry(txpos, tcp_pkt_t, list);
                if (txpkt->num_transmissions == 0) {
                    tcp_outbox_transmit(tcp, txpkt);
                }
            }
        }

        /* We got an ACK for our FIN */
        if (ohdr->fin) {
            if (tcp_in_state(tcp, FIN_WAIT_1)) {
                tcp_set_state(tcp, FIN_WAIT_2);

                /*
                 * We also start the FIN timeout when entering FIN_WAIT_2 state,
                 * to prevent a situation where the socket is closed locally and
                 * the remote sender dies - we would be waiting forever for the
                 * remote peer to send its FIN.
                 */
                tcp_start_fin_timeout(tcp);
            } else if (tcp_in_state(tcp, CLOSING)) {
                tcp_set_state(tcp, TIME_WAIT);
                tcp_start_fin_timeout(tcp);
            } else if (tcp_in_state(tcp, LAST_ACK)) {
                tcp_set_state(tcp, CLOSED);
            } else if (tcp_in_state(tcp, TIME_WAIT)) {
                tcp_start_fin_timeout(tcp);
            }
        }

        /* No longer need to keep track of this packet! */
        list_del(pos);
        skb_release(oskb);
        timer_cancel(&opkt->retransmit_timer);
        free(opkt);
        num_acked++;
    }

    /* If we get three duplicate ACKs, retransmit earliest packet */
    if (num_acked == 0 && !list_empty(&tcp->outbox)) {
        if (++tcp->num_duplicate_acks == 3) {
            tcp_debugf("Retransmitting earliest packet\n");
            tcp_outbox_transmit(tcp, list_first_entry(&tcp->outbox, tcp_pkt_t, list));
            tcp->num_duplicate_acks = 0;
        }
    } else {
        tcp->num_duplicate_acks = 0;
    }

    return num_acked;
}

/*
 * Handles an incoming packet to a connected socket.
 */
static int
tcp_handle_rx_connected(tcp_sock_t *tcp, skb_t *skb)
{
    assert(!tcp_in_state(tcp, LISTEN));
    tcp_hdr_t *hdr = skb_transport_header(skb);

    /* If socket is closed, reply with RST */
    if (tcp_in_state(tcp, CLOSED)) {
        tcp_debugf("Received packet to closed socket\n");
        if (!hdr->rst) {
            tcp_reply_rst(net_sock(tcp)->iface, skb);
        }
        return 0;
    }

    /*
     * Special handshake processing for SYN_SENT state.
     */
    if (tcp_in_state(tcp, SYN_SENT)) {
        /*
         * If the ACK is clearly bogus, reset the connection and
         * reply with RST.
         */
        if (hdr->ack && !tcp_is_ack_valid(tcp, hdr)) {
            tcp_debugf("Bogus ACK received in SYN_SENT state\n");
            if (!hdr->rst) {
                tcp_reply_rst(net_sock(tcp)->iface, skb);
            }
            tcp->reset = true;
            tcp_set_state(tcp, CLOSED);
            tcp_release(tcp);
            return 0;
        }

        /*
         * If remote requested a reset and the ACK is current,
         * grant their wish. Otherwise, ignore the reset.
         */
        if (hdr->rst) {
            tcp_debugf("Received RST in SYN_SENT state\n");
            if (hdr->ack && tcp_is_ack_current(tcp, hdr)) {
                tcp->reset = true;
                tcp_set_state(tcp, CLOSED);
                tcp_release(tcp);
            }
            return 0;
        }

        /*
         * Packet seems to be valid, let's handle the SYN now.
         */
        if (hdr->syn) {
            tcp->ack_num = seq(hdr) + 1;
            tcp->read_num = tcp->ack_num;

            /* Handle ACK in this packet (likely a SYN-ACK) */
            if (hdr->ack) {
                tcp_handle_rx_ack(tcp, ack(hdr));
            }

            /* Insert packet into inbox in case there's data to process */
            tcp_inbox_insert(tcp, skb);

            /*
             * If our SYN got ACKed, we should already be in the
             * ESTABLISHED state. If we're still in SYN_SENT, that
             * means we have a double-open scenario. As per the spec,
             * transition to SYN_RECEIVED state and retransmit SYN
             * (which will now become a SYN-ACK).
             */
            if (tcp_in_state(tcp, SYN_SENT)) {
                tcp_set_state(tcp, SYN_RECEIVED);
                tcp_pkt_t *syn = list_first_entry(&tcp->outbox, tcp_pkt_t, list);
                tcp_outbox_transmit(tcp, syn);
            } else {
                tcp_send_ack(tcp);
            }

            return 0;
        }

        tcp_debugf("Unhandled packet in SYN_SENT state, dropping\n");
        return 0;
    }

    /*
     * If the segment is outside of the receive window,
     * discard it and send an ACK if no RST. Note that
     * we still process ACKs, so we don't return immediately.
     */
    bool in_rwnd = tcp_in_rwnd(tcp, skb);
    if (!in_rwnd) {
        tcp_debugf("Packet outside receive window\n");
        if (!hdr->rst) {
            tcp_send_ack(tcp);
        }
    } else {
        /*
         * Handle RST (we use the sequence number instead of
         * ack number here, which is checked above).
         */
        if (hdr->rst) {
            tcp_debugf("Received RST in middle of connection\n");
            tcp->reset = true;
            tcp_set_state(tcp, CLOSED);
            tcp_release(tcp);
            return 0;
        }

        /*
         * If we got a SYN in the middle of the connection,
         * reset the connection.
         */
        if (hdr->syn) {
            tcp_debugf("Received SYN in middle of connection\n");
            tcp->reset = true;
            tcp_reply_rst(net_sock(tcp)->iface, skb);
            tcp_set_state(tcp, CLOSED);
            tcp_release(tcp);
            return 0;
        }
    }

    /*
     * As per RFC793, if there's no ACK, we drop the segment
     * even if there's data in it.
     */
    if (!hdr->ack) {
        tcp_debugf("No ACK in packet, dropping\n");
        return 0;
    }

    /*
     * Handle invalid ACKs. If we're in the SYN_RECEIVED state,
     * we can only have sent a SYN ourselves, so anything that's
     * outside the window is invalid. According to the spec, we
     * send an ACK if we get an invalid ACK otherwise.
     */
    if (tcp_in_state(tcp, SYN_RECEIVED)) {
        if (!tcp_is_ack_valid(tcp, hdr) || !tcp_is_ack_current(tcp, hdr)) {
            tcp_debugf("Invalid ACK in SYN_RECEIVED state\n");
            tcp_reply_rst(net_sock(tcp)->iface, skb);
            return 0;
        }
    } else {
        if (!tcp_is_ack_valid(tcp, hdr)) {
            tcp_debugf("Invalid ACK\n");
            tcp_send_ack(tcp);
            return 0;
        }
    }

    /*
     * Handle the ACK. If that causes us to transition to the
     * CLOSED state, terminate the connection.
     */
    tcp_handle_rx_ack(tcp, ack(hdr));
    if (tcp_in_state(tcp, CLOSED)) {
        tcp_release(tcp);
        return 0;
    }

    /*
     * Now insert packet into inbox. This will also do any
     * processing of FIN flags, once the segment containing
     * the FIN is in-order.
     */
    if (in_rwnd && tcp_in_state(tcp, ESTABLISHED | FIN_WAIT_1 | FIN_WAIT_2)) {
        tcp_inbox_insert(tcp, skb);
    }

    /*
     * And finally, send an ACK if there was new data in this
     * segment.
     */
    if (tcp_seg_len(skb) > 0) {
        tcp_send_ack(tcp);
    }

    return 0;
}

/*
 * Handles an incoming packet to a listening socket.
 * The iface parameter is required since the socket
 * may be bound to all interfaces, unlike connected
 * sockets.
 */
static int
tcp_handle_rx_listening(net_iface_t *iface, tcp_sock_t *tcp, skb_t *skb)
{
    assert(tcp_in_state(tcp, LISTEN));
    tcp_hdr_t *hdr = skb_transport_header(skb);

    /* Ignore incoming RSTs */
    if (hdr->rst) {
        return 0;
    }

    /* ACK to a LISTEN socket -> reply with RST */
    if (hdr->ack) {
        tcp_debugf("Received ACK to listening socket\n");
        return tcp_reply_rst(iface, skb);
    }

    /* New incoming connection! */
    if (hdr->syn) {
        /* Create a new socket */
        net_sock_t *connsock = socket_obj_alloc(SOCK_TCP);
        if (connsock == NULL) {
            return -1;
        }

        /*
         * Bind and connect socket (bypass conflict checks, since
         * a TCP socket is identified by both local and remote
         * addresses, and listening sockets cannot be connected).
         */
        ip_hdr_t *iphdr = skb_network_header(skb);
        connsock->bound = true;
        connsock->iface = iface;
        connsock->local.ip = iphdr->dest_ip;
        connsock->local.port = ntohs(hdr->be_dest_port);
        connsock->connected = true;
        connsock->remote.ip = iphdr->src_ip;
        connsock->remote.port = ntohs(hdr->be_src_port);

        /* Transition to SYN-received state */
        tcp_sock_t *conntcp = tcp_sock(connsock);
        conntcp->ack_num = seq(hdr) + 1;
        conntcp->read_num = conntcp->ack_num;
        tcp_acquire(conntcp);
        tcp_set_state(conntcp, SYN_RECEIVED);

        /* Add packet to inbox in case it contains data */
        tcp_inbox_insert(conntcp, skb);

        /* Send our initial SYN-ACK */
        if (tcp_send_syn(conntcp) < 0) {
            tcp_set_state(conntcp, CLOSED);
            tcp_release(conntcp);
            return -1;
        }

        /* Add socket to backlog for accept() */
        list_add_tail(&conntcp->backlog, &tcp->backlog);
        return 0;
    }

    /* Drop everything else */
    return 0;
}

/* Handles reception of a TCP packet */
int
tcp_handle_rx(net_iface_t *iface, skb_t *skb)
{
    /* Pop header */
    if (!skb_may_pull(skb, sizeof(tcp_hdr_t))) {
        tcp_debugf("TCP packet too small: cannot pull header\n");
        return -1;
    }
    tcp_hdr_t *hdr = skb_reset_transport_header(skb);
    skb_pull(skb, sizeof(tcp_hdr_t));

    /* Pop and ignore options */
    int options_len = hdr->data_offset * 4 - sizeof(tcp_hdr_t);
    if (!skb_may_pull(skb, options_len)) {
        tcp_debugf("TCP packet too small: cannot pull options\n");
        return -1;
    }
    skb_pull(skb, options_len);

    /* If debugging is enabled, randomly drop some packets */
#if TCP_DEBUG_DROP
    if (rand() % 100 < TCP_DEBUG_RX_DROP_FREQ) {
        tcp_dump_pkt("recv (dropped)", skb);
        return 0;
    }
#endif

    /* Dump packet contents */
    tcp_dump_pkt("recv", skb);

    ip_hdr_t *iphdr = skb_network_header(skb);
    ip_addr_t dest_ip = iphdr->dest_ip;
    ip_addr_t src_ip = iphdr->src_ip;
    uint16_t dest_port = ntohs(hdr->be_dest_port);
    uint16_t src_port = ntohs(hdr->be_src_port);

    /* Try to dispatch to a connected socket */
    net_sock_t *sock = get_sock_by_addr(SOCK_TCP, dest_ip, dest_port, src_ip, src_port);
    if (sock != NULL) {
        return tcp_handle_rx_connected(tcp_sock(sock), skb);
    }

    /* No connected socket? Okay, try to dispatch to a listening socket */
    sock = get_sock_by_addr(SOCK_TCP, dest_ip, dest_port, ANY_IP, 0);
    if (sock != NULL && sock->listening) {
        return tcp_handle_rx_listening(iface, tcp_sock(sock), skb);
    }

    /* No socket, reply with RST */
    if (hdr->rst) {
        return 0;
    } else {
        return tcp_reply_rst(iface, skb);
    }
}

/* TCP socket constructor */
int
tcp_ctor(net_sock_t *sock)
{
    tcp_sock_t *tcp = malloc(sizeof(tcp_sock_t));
    if (tcp == NULL) {
        tcp_debugf("Cannot allocate space for TCP data\n");
        return -1;
    }

    tcp->sock = sock;
    tcp->state = CLOSED;
    list_init(&tcp->backlog);
    list_init(&tcp->inbox);
    list_init(&tcp->outbox);
    timer_init(&tcp->fin_timer);
    tcp->rwnd_size = TCP_RWND_SIZE;
    tcp->read_num = 0;
    tcp->ack_num = 0;
    tcp->seq_num = tcp_rand32();
    tcp->unack_num = tcp->seq_num;
    tcp->num_duplicate_acks = 0;
    tcp->reset = false;
    tcp->estimated_rtt = -1;
    tcp->variance_rtt = -1;
    sock->private = tcp;
    return 0;
}

/* TCP socket destructor */
void
tcp_dtor(net_sock_t *sock)
{
    tcp_sock_t *tcp = tcp_sock(sock);
    list_t *pos, *next;

    /* If this is a listening socket, terminate all pending connections */
    if (sock->listening) {
        list_for_each_safe(pos, next, &tcp->backlog) {
            tcp_sock_t *pending = list_entry(pos, tcp_sock_t, backlog);
            tcp_set_state(pending, FIN_WAIT_1);

            /* Try sending a FIN. If that fails, abort the hard way */
            if (tcp_send_fin(pending) < 0) {
                tcp_set_state(pending, CLOSED);
                tcp_release(pending);
            }
        }
    }

    /* Clear inbox */
    list_for_each_safe(pos, next, &tcp->inbox) {
        skb_t *skb = list_entry(pos, skb_t, list);
        skb_release(skb);
    }

    /* Clear outbox */
    list_for_each_safe(pos, next, &tcp->outbox) {
        tcp_pkt_t *pkt = list_entry(pos, tcp_pkt_t, list);
        skb_release(pkt->skb);
        timer_cancel(&pkt->retransmit_timer);
        free(pkt);
    }

    tcp_debugf("Destroyed TCP socket 0x%08x\n", tcp);
    timer_cancel(&tcp->fin_timer);
    free(tcp);
}

/*
 * bind() socketcall handler. Only works on sockets that
 * have not yet been put into listening mode.
 */
int
tcp_bind(net_sock_t *sock, const sock_addr_t *addr)
{
    /* Can't bind connected or listening sockets */
    if (sock->connected || sock->listening) {
        return -1;
    }

    /* Copy address into kernelspace */
    sock_addr_t tmp;
    if (!copy_from_user(&tmp, addr, sizeof(sock_addr_t))) {
        return -1;
    }

    return socket_bind_addr(sock, tmp.ip, tmp.port);
}

/*
 * connect() socketcall handler. Only works on non-listening
 * sockets that have not already been connected. Sends a SYN
 * to the specified remote address.
 */
int
tcp_connect(net_sock_t *sock, const sock_addr_t *addr)
{
    /* Cannot connect already-connected or listening sockets */
    if (sock->connected || sock->listening) {
        return -1;
    }

    /* Socket must be closed at this point */
    tcp_sock_t *tcp = tcp_sock(sock);
    assert(tcp_in_state(tcp, CLOSED));

    /* Copy address to kernelspace */
    sock_addr_t tmp;
    if (!copy_from_user(&tmp, addr, sizeof(sock_addr_t))) {
        return -1;
    }

    /* Attempt to connect */
    if (socket_connect_addr(sock, tmp.ip, tmp.port) < 0) {
        tcp_debugf("Could not connect socket\n");
        return -1;
    }

    /* Auto-bind socket if not done already */
    bool autobound = false;
    if (!sock->bound) {
        if (socket_bind_addr(sock, ANY_IP, 0) < 0) {
            sock->connected = false;
            tcp_debugf("Could not auto-bind socket\n");
            return -1;
        }
        autobound = true;
    }

    /* Send our SYN packet */
    tcp_acquire(tcp);
    tcp_set_state(tcp, SYN_SENT);
    if (tcp_send_syn(tcp) < 0) {
        tcp_debugf("Could not send SYN\n");
        sock->connected = false;
        if (autobound) {
            sock->bound = false;
        }
        tcp_set_state(tcp, CLOSED);
        tcp_release(tcp);
    }

    return 0;
}

/*
 * listen() socketcall handler. Puts the socket into listening
 * mode. Only works on unconnected sockets.
 */
int
tcp_listen(net_sock_t *sock, int backlog)
{
    /* Cannot call listen() on a unbound or connected socket */
    if (!sock->bound || sock->connected) {
        return -1;
    } else if (sock->listening) {
        return 0;
    }

    /* Socket must be closed at this point */
    tcp_sock_t *tcp = tcp_sock(sock);
    assert(tcp_in_state(tcp, CLOSED));

    /* Transition from CLOSED -> LISTEN state */
    sock->listening = true;
    tcp_acquire(tcp);
    tcp_set_state(tcp, LISTEN);
    return 0;
}

/*
 * accept() socketcall handler. Accepts a single incoming
 * TCP connection. Copies the remote endpoint's address
 * into addr.
 */
int
tcp_accept(net_sock_t *sock, sock_addr_t *addr)
{
    /* Cannot call accept() on a non-listening socket */
    if (!sock->listening) {
        return -1;
    }

    /* Check that socket is still open */
    tcp_sock_t *tcp = tcp_sock(sock);
    if (!tcp_in_state(tcp, LISTEN)) {
        return -1;
    }

    /* Check if we have anything in the backlog */
    if (list_empty(&tcp->backlog)) {
        return -EAGAIN;
    }

    /* Pop first entry from the backlog */
    tcp_sock_t *conntcp = list_first_entry(&tcp->backlog, tcp_sock_t, backlog);
    net_sock_t *connsock = net_sock(conntcp);

    /* Copy address to userspace */
    if (addr != NULL && !copy_to_user(addr, &connsock->remote, sizeof(sock_addr_t))) {
        return -1;
    }

    /* Bind the socket to a file */
    int fd = socket_obj_bind_file(get_executing_files(), connsock);
    if (fd < 0) {
        return -1;
    }

    /* Consume socket from backlog */
    list_del(&conntcp->backlog);
    return fd;
}

/*
 * recvfrom() socketcall handler. Reads the specified number of
 * bytes from the remote endpoint. addr is ignored.
 */
int
tcp_recvfrom(net_sock_t *sock, void *buf, int nbytes, sock_addr_t *addr)
{
    /* Standard error checks */
    if (nbytes < 0 || !sock->connected) {
        return -1;
    }

    /*
     * If the socket is closed due to an error (reset),
     * reading from it is a failure. If it's closed but under
     * normal conditions, let the user keep reading from the
     * socket (this can occur if user calls shutdown() followed
     * by read()).
     */
    tcp_sock_t *tcp = tcp_sock(sock);
    if (tcp_in_state(tcp, CLOSED) && tcp->reset) {
        return -1;
    } else if (tcp_in_state(tcp, SYN_SENT | SYN_RECEIVED)) {
        return -EAGAIN;
    }

    uint16_t original_rwnd = tcp_rwnd_size(tcp);
    uint8_t *bufp = buf;
    int copied = 0;
    while (!list_empty(&tcp->inbox)) {
        skb_t *skb = list_first_entry(&tcp->inbox, skb_t, list);
        tcp_hdr_t *hdr = skb_transport_header(skb);

        /*
         * If this packet hasn't been ACKed yet, we must have a hole,
         * so stop here.
         */
        uint32_t pkt_seq_num = seq(hdr);
        if (cmp(pkt_seq_num, tcp->ack_num) > 0) {
            break;
        }

        /* Consume SYN flag */
        if (hdr->syn && tcp->read_num == pkt_seq_num) {
            tcp->read_num++;
        }

        /*
         * Find starting byte, based on how much we've already read
         * and whether this segment contains a SYN (SYNs take up
         * one sequence number but no bytes).
         */
        int seq_offset = tcp->read_num - pkt_seq_num;
        int byte_offset = hdr->syn ? seq_offset - 1 : seq_offset;
        int bytes_remaining = tcp_body_len(skb) - byte_offset;
        if (bytes_remaining >= 0) {
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
             * If we didn't copy the entire body, user buffer must have
             * been too small. Stop here and try again next time. Do not
             * free the SKB, in case there's more data left in it.
             */
            if (bytes_to_copy < bytes_remaining) {
                break;
            }
        }

        tcp_inbox_remove(tcp, skb);
        skb_release(skb);
    }

    /*
     * Only advertise window updates when we have at least
     * one MSS worth of window space, to prevent silly window
     * syndrome countermeasures on the receiver side from
     * ignoring our window update.
     */
    if (original_rwnd < TCP_MAX_LEN && tcp_rwnd_size(tcp) >= TCP_MAX_LEN) {
        if (!tcp_in_state(tcp, TIME_WAIT | CLOSE_WAIT | LAST_ACK | CLOSED)) {
            tcp_send_ack(tcp);
        }
    }

    /*
     * If we didn't copy anything and we're in a closing state, there's
     * no more data in the stream to read. Otherwise, it just means
     * we didn't get any data yet, so return -EAGAIN.
     */
    if (copied == 0) {
        if (tcp_in_state(tcp, TIME_WAIT | CLOSE_WAIT | LAST_ACK | CLOSED)) {
            return 0;
        } else {
            return -EAGAIN;
        }
    }

    return copied;
}

/*
 * sendto() socketcall handler. Splits the input buffer into
 * TCP packets and sends them to the remote endpoint. Fails if
 * the writing end of the socket is closed. addr is ignored.
 */
int
tcp_sendto(net_sock_t *sock, const void *buf, int nbytes, const sock_addr_t *addr)
{
    /* Standard error checks */
    if (nbytes < 0 || !sock->connected) {
        return -1;
    }

    /* Check that socket write end is still open */
    tcp_sock_t *tcp = tcp_sock(sock);
    if (tcp_in_state(tcp, CLOSED | FIN_WAIT_1 | FIN_WAIT_2 | CLOSING | TIME_WAIT)) {
        return -1;
    } else if (tcp_in_state(tcp, SYN_SENT | SYN_RECEIVED)) {
        return -EAGAIN;
    }

    /* Don't return -1 if nothing to copy */
    if (nbytes == 0) {
        return 0;
    }

    const uint8_t *bufp = buf;
    int sent = 0;
    while (sent < nbytes) {
        /* Split into MSS packets */
        int body_len = nbytes - sent;
        if (body_len > TCP_MAX_LEN) {
            body_len = TCP_MAX_LEN;
        }

        /* Create new SKB for packet */
        skb_t *skb = tcp_alloc_skb(body_len);
        if (skb == NULL) {
            break;
        }

        /* Copy data into SKB */
        uint8_t *body = skb_put(skb, body_len);
        if (!copy_from_user(body, &bufp[sent], body_len)) {
            skb_release(skb);
            break;
        }

        /* Initialize packet */
        tcp_hdr_t *hdr = skb_transport_header(skb);
        hdr->be_src_port = htons(sock->local.port);
        hdr->be_dest_port = htons(sock->remote.port);
        hdr->be_seq_num = htonl(tcp->seq_num);
        tcp->seq_num += body_len;

        /* Insert packet into outbox */
        tcp_pkt_t *pkt = tcp_outbox_insert(tcp, skb);
        if (pkt == NULL) {
            skb_release(skb);
            break;
        }

        /*
         * Transmit packet immediately (remember, we have
         * no regard for the receiver's rwnd or our cwnd,
         * because we're lazy ;-)
         */
        tcp_outbox_transmit(tcp, pkt);
        skb_release(skb);
        sent += body_len;
    }

    /*
     * No bytes sent indicates complete failure;
     * 0 < sent < nbytes indicates partial failure.
     */
    if (sent == 0) {
        return -1;
    } else {
        return sent;
    }
}

/*
 * shutdown() socketcall handler. Sends a FIN to the
 * remote endpoint and closes the writing end of the socket.
 */
int
tcp_shutdown(net_sock_t *sock)
{
    tcp_sock_t *tcp = tcp_sock(sock);
    if (!sock->connected) {
        tcp_debugf("shutdown() called on an unconnected socket\n");
        return -1;
    }

    tcp_close_write(tcp);
    return 0;
}

/*
 * close() socketcall handler. Sends a FIN to the
 * remote endpoint and closes the writing end of the
 * socket. The socket will be inaccessible from userspace,
 * but will remain alive in the kernel until the FIN
 * has been ACK'd.
 */
int
tcp_close(net_sock_t *sock)
{
    tcp_sock_t *tcp = tcp_sock(sock);
    tcp_inbox_drain(tcp);
    tcp_close_write(tcp);
    return 0;
}
