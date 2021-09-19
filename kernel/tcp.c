/*
 * Note: This implementation of TCP is not standards-compliant.
 * It does not fully implement the following features:
 *
 * - URG flag
 * - TCP options (window scale, etc)
 * - Congestion control
 * - Delayed ACK
 */
#include "tcp.h"
#include "types.h"
#include "debug.h"
#include "string.h"
#include "list.h"
#include "file.h"
#include "pit.h"
#include "timer.h"
#include "syscall.h"
#include "myalloc.h"
#include "socket.h"
#include "paging.h"
#include "net.h"
#include "ip.h"
#include "ethernet.h"
#include "mt19937.h"

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
 * Time in milliseconds to wait in the TIME_WAIT and FIN_WAIT_2 states
 * before releasing the socket.
 */
#define TCP_FIN_TIMEOUT_MS 60000

/*
 * Maximum number of times to attempt retransmitting a packet
 * before killing the connection.
 */
#define TCP_MAX_RETRANSMISSIONS 3

/* Allowed RTO range in milliseconds for retransmission timer */
#define TCP_MIN_RTO_MS 1000
#define TCP_MAX_RTO_MS 60000
#define TCP_INIT_RTO_MS 1000

/* Starting receive/send window size. Must be >= TCP_MAX_LEN. */
#define TCP_INIT_WND_SIZE 8192

/* State of a TCP connection */
typedef enum {
    LISTEN       = (1 << 0),  /* Waiting for SYN */
    SYN_SENT     = (1 << 1),  /* SYN sent, waiting for SYN-ACK */
    SYN_RECEIVED = (1 << 2),  /* SYN received, waiting for ACK */
    ESTABLISHED  = (1 << 3),  /* Three-way handshake complete */
    FIN_WAIT_1   = (1 << 4),  /* shutdown() */
    FIN_WAIT_2   = (1 << 5),  /* shutdown() -> ACK received */
    CLOSING      = (1 << 6),  /* shutdown() -> FIN received */
    TIME_WAIT    = (1 << 7),  /* shutdown() -> FIN received, ACK received */
    CLOSE_WAIT   = (1 << 8),  /* FIN received */
    LAST_ACK     = (1 << 9),  /* FIN received -> shutdown() */
    CLOSED       = (1 << 10), /* Used when the connection is closed but file is still open */
} tcp_state_t;

/* TCP socket state */
typedef struct {
    /* Back-pointer to the socket object */
    net_sock_t *sock;

    /* Current state of the connection */
    tcp_state_t state;

    /*
     * If this is a listening socket, this holds the head of the backlog list.
     * If this is a connected socket, this holds our node in the listening
     * socket's backlog list. For a connected socket that has already been
     * accepted, this field is unused.
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
     * This holds our node in the pending ACK queue.
     */
    list_t ack_queue;

    /*
     * Timer for the TIME_WAIT and FIN_WAIT_2 states. When this timer expires,
     * the socket is released.
     */
    timer_t fin_timer;

    /*
     * Retransmission timer - will retransmit the first packet in the outbox
     * when it expires. Renewed on every ACK we get that advances the
     * send_unack_num. Canceled when the outbox is empty.
     */
    timer_t rto_timer;

    /*
     * Number of remaining slots in the connection backlog, for listening
     * sockets.
     */
    int backlog_capacity;

    /*
     * Receive window size of the socket. Used to limit the number of packets
     * the peer sends to us at once, so we don't OOM. This value may be
     * negative to indicate that our inbox is fuller than normal; when reading
     * a negative value, it should be treated as zero.
     */
    int recv_wnd_size;

    /*
     * Sequence number in our inbox that userspace has consumed up to. Used
     * to keep track of which bytes need to be copied from the inbox to
     * userspace on the next recvfrom() call. May be in the middle of a
     * packet, in the case of partial reads.
     */
    uint32_t recv_read_num;

    /*
     * Sequence number of next in-order packet we expect from the remote
     * endpoint. Used to set the ACK field on outgoing packets.
     */
    uint32_t recv_next_num;

    /*
     * Sequence number of next packet to be added to the outbox (i.e.
     * sequence number + segment length of the last packet in our
     * outbox). Used to generate the sequence number on new outbound
     * packets.
     */
    uint32_t send_next_num;

    /*
     * Sequence number of the first packet that has not been acknowledged
     * yet (i.e. sequence number of the first packet in our outbox).
     */
    uint32_t send_unack_num;

    /*
     * Send window size, and the seq + ack numbers used to last update the
     * window size.
     */
    uint32_t send_wnd_seq;
    uint32_t send_wnd_ack;
    uint16_t send_wnd_size;

    /* Duplicate ACK counter for fast retransmission */
    uint8_t num_duplicate_acks : 2;

    /* Whether the connection has been reset and cannot be read from */
    bool reset : 1;

    /*
     * Whether the socket is no longer accessible to userspace and thus
     * will never be read from again. When this is true, the kernel will
     * discard all incoming data as if userspace had read it.
     */
    bool read_closed : 1;

    /* Retransmission timer values, in milliseconds */
    int estimated_rtt;
    int variance_rtt;
    int rto;
} tcp_sock_t;

/*
 * Packet in the TCP outbox. Used to track timestamps for measuring RTT.
 */
typedef struct {
    list_t list;
    tcp_sock_t *tcp;
    skb_t *skb;

    /*
     * Number of times we've transmitted this packet, including both
     * fast retransmissions (3-ACK) and retransmission timeouts.
     */
    int num_transmissions;

    /*
     * Monotonic time at which we last transmitted this packet, used
     * to update the RTT when we receive the ACK for this packet.
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
#define cmp(a, b) ((int32_t)((a) - (b)))
#define ack(hdr) (ntohl((hdr)->be_ack_num))
#define seq(hdr) (ntohl((hdr)->be_seq_num))

/* List of TCP sockets that have an ACK enqueued */
static list_declare(ack_queue);

/* Forward declaration */
static void tcp_start_rto_timeout(tcp_sock_t *tcp);

/*
 * Increments the TCP socket reference count.
 */
static tcp_sock_t *
tcp_acquire(tcp_sock_t *tcp)
{
    socket_obj_retain(net_sock(tcp));
    return tcp;
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
    socket_obj_release(net_sock(tcp));
}

/*
 * Returns the body length of the given TCP packet.
 */
static int
tcp_body_len(skb_t *skb)
{
    tcp_hdr_t *hdr = skb_transport_header(skb);
    int tcp_hdr_len = hdr->data_offset * 4;
    char *pkt_body = (char *)hdr + tcp_hdr_len;
    return (char *)skb_tail(skb) - pkt_body;
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
__unused static const char *
tcp_get_state_str(tcp_state_t state)
{
    static const char *names[] = {
#define ENTRY(x) #x
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
    return names[ctz(state)];
}

/*
 * Sets the state of a TCP connection. Increments the
 * reference count if the connection transitions away from
 * CLOSED state, and decrements it if transitioning to the
 * CLOSED state.
 */
static void
tcp_set_state(tcp_sock_t *tcp, tcp_state_t state)
{
    tcp_debugf(
        "TCP state (0x%08x) %s -> %s\n",
        tcp,
        tcp_get_state_str(tcp->state),
        tcp_get_state_str(state));

    if (tcp->state == state) {
        return;
    } else if (tcp->state == CLOSED) {
        tcp_acquire(tcp);
    } else if (state == CLOSED) {
        tcp_release(tcp);
    }

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
 * Returns the receive window size of the TCP connection.
 */
static uint16_t
tcp_rwnd_size(tcp_sock_t *tcp)
{
    if (tcp->recv_wnd_size < 0) {
        return 0;
    }
    return tcp->recv_wnd_size;
}

/*
 * Returns whether the packet is within our receive window,
 * that is, some portion of it lies between our next expected
 * sequence number and the maximum expected sequence number.
 */
static bool
tcp_in_rwnd(tcp_sock_t *tcp, uint32_t seq_num, int seg_len)
{
    uint16_t rwnd_size = tcp_rwnd_size(tcp);
    uint32_t ack_num = tcp->recv_next_num;

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
 * Allocates and partially initializes a new TCP packet.
 * The caller must set the src/dest ports and the seq
 * number, along with any flags, before sending the packet.
 */
static skb_t *
tcp_alloc_skb(int body_len)
{
    assert(body_len >= 0);

    int hdr_len = sizeof(tcp_hdr_t) + sizeof(ip_hdr_t) + sizeof(ethernet_hdr_t);
    skb_t *skb = skb_alloc(hdr_len + body_len);
    if (skb == NULL) {
        return NULL;
    }

    skb_reserve(skb, hdr_len);
    tcp_hdr_t *hdr = skb_push(skb, sizeof(tcp_hdr_t));
    skb_set_transport_header(skb);
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
 * Sends a TCP packet to the specified destination. This will
 * compute the checksum field, but NOT update the ACK or window
 * fields.
 */
static int
tcp_send_raw(net_iface_t *iface, ip_addr_t dest_ip, skb_t *skb)
{
    assert(iface != NULL);

    /*
     * Determine next-hop IP address. Note that the interface
     * is guaranteed to not change (assuming net_route() doesn't
     * fail and return NULL).
     */
    ip_addr_t neigh_ip;
    iface = net_route(iface, dest_ip, &neigh_ip);
    if (iface == NULL) {
        return -1;
    }

    /* Re-compute checksum */
    tcp_hdr_t *hdr = skb_transport_header(skb);
    hdr->be_checksum = htons(0);
    hdr->be_checksum = ip_pseudo_checksum(
        skb, iface->ip_addr, dest_ip, IPPROTO_TCP);

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
 * Sends a TCP packet to the connected remote peer. This will
 * set the ACK and window fields based on the state of the socket.
 * The socket must not be in the CLOSED state.
 */
static int
tcp_send(tcp_sock_t *tcp, skb_t *skb)
{
    assert(!tcp_in_state(tcp, CLOSED));

    net_sock_t *sock = net_sock(tcp);
    tcp_hdr_t *hdr = skb_transport_header(skb);

    /*
     * CLOSED and SYN_SENT are the only two states in which we
     * don't know the remote peer's sequence number.
     */
    if (!tcp_in_state(tcp, SYN_SENT)) {
        hdr->ack = 1;
        hdr->be_ack_num = htonl(tcp->recv_next_num);
    }

    hdr->be_window_size = htons(tcp_rwnd_size(tcp));

    return tcp_send_raw(sock->iface, sock->remote.ip, skb);
}

/*
 * Creates and sends a new empty ACK packet to the remote peer.
 * This does NOT add any packets to the outbox.
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
    hdr->be_seq_num = htonl(tcp->send_next_num);

    /* Don't enqueue packet, just directly send empty ACK */
    int ret = tcp_send(tcp, skb);
    skb_release(skb);
    return ret;
}

/*
 * Replies to an incoming packet with a RST packet. Since we won't
 * always have a TCP socket, this takes the interface we received
 * the packet on instead, and infers the rest of the arguments from
 * the original packet. This does NOT add any packets to the outbox.
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
    int ret = tcp_send_raw(iface, orig_iphdr->src_ip, skb);
    skb_release(skb);
    return ret;
}

/*
 * Initializes socket fields that depend on the remote sequence number.
 */
static void
tcp_init_remote_seq(tcp_sock_t *tcp, uint32_t seq_num)
{
    tcp->recv_next_num = seq_num;
    tcp->recv_read_num = seq_num;
    tcp->send_wnd_seq = seq_num;
}

/*
 * Adds a socket to the pending ACK queue. ACKs are sent at the
 * end of an interrupt, which lets us merge ACKs for packets received
 * in the same interrupt.
 */
static void
tcp_enqueue_ack(tcp_sock_t *tcp)
{
    if (list_empty(&tcp->ack_queue)) {
        list_add(&tcp->ack_queue, &ack_queue);
    }
}

/*
 * Delivers all pending ACKs.
 */
void
tcp_deliver_ack(void)
{
    list_t *pos, *next;
    list_for_each_safe(pos, next, &ack_queue) {
        tcp_sock_t *tcp = tcp_acquire(list_entry(pos, tcp_sock_t, ack_queue));
        if (!tcp_in_state(tcp, CLOSED)) {
            tcp_send_ack(tcp);
        }
        list_del(&tcp->ack_queue);
    }
}

/*
 * Updates the socket RTT statistics with the given sampled RTT time,
 * and recomputes an appropriate retransmit timeout.
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
    tcp->variance_rtt = ((3 * tcp->variance_rtt) / 4) + (error / 4);
    tcp->estimated_rtt = ((7 * tcp->estimated_rtt) / 8) + (sample_rtt / 8);

    /* RTO = EstRTT + 4*VarRTT, clamped to [MIN_RTO, MAX_RTO] range */
    int rto = tcp->estimated_rtt + 4 * tcp->variance_rtt;
    if (rto < TCP_MIN_RTO_MS) {
        rto = TCP_MIN_RTO_MS;
    } else if (rto > TCP_MAX_RTO_MS) {
        rto = TCP_MAX_RTO_MS;
    }
    tcp->rto = rto;

    assert(tcp->estimated_rtt >= 0);
    assert(tcp->variance_rtt >= 0);
    assert(tcp->rto >= 0);
}

/*
 * Doubles the retransmission timeout, up to the max (60s). Called
 * when the retramission timer expires.
 */
static void
tcp_add_backoff(tcp_sock_t *tcp)
{
    if (tcp->rto >= TCP_MAX_RTO_MS / 2) {
        tcp->rto = TCP_MAX_RTO_MS;
    } else {
        tcp->rto *= 2;
    }
}

/*
 * Transmits a packet that was already in the outbox. Does not
 * check that the packet is within the send window. Starts the
 * retransmission timer if it is not already running.
 */
static int
tcp_outbox_transmit_one(tcp_sock_t *tcp, tcp_pkt_t *pkt)
{
    assert(!list_empty(&pkt->list));

    if (++pkt->num_transmissions > TCP_MAX_RETRANSMISSIONS) {
        tcp_debugf("Too many retransmissions, giving up\n");
        tcp->reset = true;
        tcp_set_state(tcp, CLOSED);
        return -1;
    }

    pkt->transmit_time = pit_monotime();
    int ret = tcp_send(tcp, pkt->skb);
    tcp_start_rto_timeout(tcp);
    return ret;
}

/*
 * Transmits all packets in the TCP outbox that are within the
 * send window and have not yet been transmitted.
 */
static int
tcp_outbox_transmit_all(tcp_sock_t *tcp)
{
    list_t *txpos;
    list_for_each(txpos, &tcp->outbox) {
        tcp_pkt_t *txpkt = list_entry(txpos, tcp_pkt_t, list);
        if (txpkt->num_transmissions == 0) {
            tcp_outbox_transmit_one(tcp, txpkt);
            if (tcp_in_state(tcp, CLOSED)) {
                return -1;
            }
        }
    }

    return 0;
}

/*
 * Called when the FIN timeout expires. Closes the socket.
 */
static void
tcp_on_fin_timeout(timer_t *timer)
{
    tcp_sock_t *tcp = tcp_acquire(timer_entry(timer, tcp_sock_t, fin_timer));
    if (tcp_in_state(tcp, CLOSED)) {
        goto exit;
    }

    tcp_debugf("FIN timeout reached, closing\n");
    tcp_set_state(tcp, CLOSED);

exit:
    tcp_release(tcp);
}

/*
 * Starts or restarts the FIN timeout.
 */
static void
tcp_restart_fin_timeout(tcp_sock_t *tcp)
{
    timer_setup(&tcp->fin_timer, TCP_FIN_TIMEOUT_MS, tcp_on_fin_timeout);
}

/*
 * Called when the retransmission timeout expires. Retransmits
 * the first un-acked packet and restarts the timer with double
 * the delay.
 */
static void
tcp_on_rto_timeout(timer_t *timer)
{
    tcp_sock_t *tcp = tcp_acquire(timer_entry(timer, tcp_sock_t, rto_timer));
    if (tcp_in_state(tcp, CLOSED) || list_empty(&tcp->outbox)) {
        goto exit;
    }

    /* Double the RTO for the next attempt */
    tcp_add_backoff(tcp);

    /* Retransmit the first packet (also re-enables the timer) */
    tcp_debugf("RTO reached, retransmitting earliest packet\n");
    tcp_pkt_t *pkt = list_first_entry(&tcp->outbox, tcp_pkt_t, list);
    tcp_outbox_transmit_one(tcp, pkt);

exit:
    tcp_release(tcp);
}

/*
 * Stops the retransmission timeout.
 */
static void
tcp_stop_rto_timeout(tcp_sock_t *tcp)
{
    timer_cancel(&tcp->rto_timer);
}

/*
 * Starts or restarts the retransmission timeout.
 */
static void
tcp_restart_rto_timeout(tcp_sock_t *tcp)
{
    timer_setup(&tcp->rto_timer, tcp->rto, tcp_on_rto_timeout);
}

/*
 * Starts the retransmission timeout if it is not already active.
 * No-op if the timeout is already active.
 */
static void
tcp_start_rto_timeout(tcp_sock_t *tcp)
{
    if (!timer_is_active(&tcp->rto_timer)) {
        timer_setup(&tcp->rto_timer, tcp->rto, tcp_on_rto_timeout);
    }
}

/*
 * Adds a packet to the TCP outbox queue. This advances the SND.NXT
 * counter. The packet is NOT transmitted.
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
    pkt->transmit_time = pit_monotime();
    list_add_tail(&pkt->list, &tcp->outbox);

    tcp->send_next_num += tcp_seg_len(skb);
    return pkt;
}

/*
 * Allocates a new, empty packet and adds it to the TCP outbox queue.
 * This advances the SND.NXT counter. The packet is NOT transmitted.
 */
static tcp_pkt_t *
tcp_outbox_insert_new(tcp_sock_t *tcp, bool syn, bool fin)
{
    tcp_pkt_t *pkt = NULL;
    net_sock_t *sock = net_sock(tcp);

    /* Allocate packet */
    skb_t *skb = tcp_alloc_skb(0);
    if (skb == NULL) {
        goto exit;
    }

    /* Initialize packet */
    tcp_hdr_t *hdr = skb_transport_header(skb);
    hdr->be_src_port = htons(sock->local.port);
    hdr->be_dest_port = htons(sock->remote.port);
    hdr->be_seq_num = htonl(tcp->send_next_num);
    hdr->syn = syn;
    hdr->fin = fin;

    /* Enqueue packet in outbox */
    pkt = tcp_outbox_insert(tcp, skb);
    if (pkt == NULL) {
        goto exit;
    }

exit:
    if (skb != NULL) {
        skb_release(skb);
    }
    return pkt;
}

/*
 * Allocates a new packet with the SYN flag set, and adds it to the
 * TCP outbox queue. This advances the SND.NXT counter. The packet
 * is NOT transmitted.
 */
static tcp_pkt_t *
tcp_outbox_insert_syn(tcp_sock_t *tcp)
{
    return tcp_outbox_insert_new(tcp, true, false);
}

/*
 * Allocates a new packet with the FIN flag set, and adds it to the
 * TCP outbox queue. This advances the SND.NXT counter. The packet
 * is NOT transmitted.
 */
static tcp_pkt_t *
tcp_outbox_insert_fin(tcp_sock_t *tcp)
{
    return tcp_outbox_insert_new(tcp, false, true);
}

/*
 * Removes a packet from the TCP outbox.
 */
static void
tcp_outbox_remove(tcp_sock_t *tcp, tcp_pkt_t *pkt)
{
    list_del(&pkt->list);
    skb_release(pkt->skb);
    free(pkt);
}

/*
 * Inserts a packet into the TCP inbox, if it is not an exact
 * duplicate of an existing packet. Returns true if the packet
 * was added, false otherwise.
 */
static bool
tcp_inbox_insert(tcp_sock_t *tcp, skb_t *skb)
{
    tcp_hdr_t *hdr = skb_transport_header(skb);
    int len = tcp_seg_len(skb);

    /*
     * Find appropriate place in the inbox queue to insert the packet.
     * Iterate from the tail since most packets probably arrive in
     * the correct order. Algorithm: find the latest position in
     * the list where SEQ(new) > SEQ(entry), and insert the new packet
     * after the existing entry. If it happens that the new packet
     * belongs at the head of the queue, we rely on the implementation
     * of list_for_each_prev to leave pos == head once the loop ends.
     */
    list_t *pos;
    list_for_each_prev(pos, &tcp->inbox) {
        skb_t *iskb = list_entry(pos, skb_t, list);
        tcp_hdr_t *ihdr = skb_transport_header(iskb);
        int c = cmp(seq(hdr), seq(ihdr));
        if (c >= 0) {
            /*
             * If this is an exact overlap with an existing segment,
             * discard it since it adds no new data.
             */
            if (c == 0 && len == tcp_seg_len(iskb)) {
                tcp_debugf("Retransmission of existing packet, dropping\n");
                return false;
            }
            break;
        }
    }
    list_add(&skb_retain(skb)->list, pos);
    return true;
}

/*
 * Removes a packet from the TCP inbox.
 */
static void
tcp_inbox_remove(tcp_sock_t *tcp, skb_t *skb)
{
    list_del(&skb->list);
    skb_release(skb);
}

/*
 * Called when a packet has been fully read (or drained).
 * Advances the read number and adjusts the window size.
 */
static void
tcp_inbox_done(tcp_sock_t *tcp, skb_t *skb)
{
    tcp_hdr_t *hdr = skb_transport_header(skb);
    assert(cmp(tcp->recv_read_num, seq(hdr)) >= 0);

    int len = tcp_seg_len(skb);
    if (cmp(tcp->recv_read_num, seq(hdr) + len) < 0) {
        tcp->recv_read_num = seq(hdr) + len;
    }

    tcp->recv_wnd_size += tcp_seg_len(skb);
    tcp_inbox_remove(tcp, skb);
}

/*
 * Used to drain the inbox if we know the user will never be
 * able to read its contents. This is used to free memory and
 * ensure the rwnd doesn't stay at zero forever.
 */
static void
tcp_inbox_drain(tcp_sock_t *tcp)
{
    assert(tcp->read_closed);

    while (!list_empty(&tcp->inbox)) {
        skb_t *skb = list_first_entry(&tcp->inbox, skb_t, list);
        tcp_hdr_t *hdr = skb_transport_header(skb);

        /*
         * If this packet hasn't been ACKed yet, we must have a hole,
         * so stop here.
         */
        if (cmp(seq(hdr), tcp->recv_next_num) > 0) {
            break;
        }

        tcp_inbox_done(tcp, skb);
    }
}

/*
 * Closes the write end of a full-duplex socket. No-op if the
 * write end is already closed. Advances the socket state.
 */
static void
tcp_close_write(tcp_sock_t *tcp)
{
    if (tcp_in_state(tcp, LISTEN | SYN_SENT)) {
        tcp_set_state(tcp, CLOSED);
    } else if (tcp_in_state(tcp, SYN_RECEIVED | ESTABLISHED)) {
        tcp_set_state(tcp, FIN_WAIT_1);
        if (tcp_outbox_insert_fin(tcp) == NULL) {
            tcp->reset = true;
            tcp_set_state(tcp, CLOSED);
        } else {
            tcp_outbox_transmit_all(tcp);
        }
    } else if (tcp_in_state(tcp, CLOSE_WAIT)) {
        tcp_set_state(tcp, LAST_ACK);
        if (tcp_outbox_insert_fin(tcp) == NULL) {
            /*
             * Since we were in close-wait state, we know there's no
             * more data from the remote peer, so this is fine. No
             * need to set the reset flag.
             */
            tcp_set_state(tcp, CLOSED);
        } else {
            tcp_outbox_transmit_all(tcp);
        }
    }
}

/*
 * Handles an incoming ACK. Removes fully acked packets from the
 * outbox, updates SND.WND, and may transmit packets that enter
 * the send window or have received duplicate ACKs. May advance
 * the socket state.
 */
static void
tcp_outbox_handle_rx_ack(tcp_sock_t *tcp, tcp_hdr_t *hdr)
{
    int num_acked = 0;
    list_t *pos, *next;
    list_for_each_safe(pos, next, &tcp->outbox) {
        tcp_pkt_t *opkt = list_entry(pos, tcp_pkt_t, list);
        skb_t *oskb = opkt->skb;
        tcp_hdr_t *ohdr = skb_transport_header(oskb);
        int olen = tcp_seg_len(oskb);

        /*
         * Since ACK is for the next expected sequence number,
         * it's only useful when SEQ(pkt) + SEG_LEN(pkt) <= ack_num.
         * If this is an ACK for this specific packet, update the RTT.
         */
        int d = cmp(seq(ohdr) + olen, ack(hdr));
        if (d > 0) {
            break;
        } else if (d == 0) {
            /*
             * Karn's algorithm: Only update RTT on packets that have
             * been transmitted once to avoid ambiguous results on
             * retransmitted packets.
             */
            if (opkt->num_transmissions == 1) {
                tcp_update_rtt(tcp, pit_monotime() - opkt->transmit_time);
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
            tcp_outbox_transmit_all(tcp);
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
                tcp_restart_fin_timeout(tcp);
            } else if (tcp_in_state(tcp, CLOSING)) {
                tcp_set_state(tcp, TIME_WAIT);
                tcp_restart_fin_timeout(tcp);
            } else if (tcp_in_state(tcp, LAST_ACK)) {
                tcp_set_state(tcp, CLOSED);
            } else if (tcp_in_state(tcp, TIME_WAIT)) {
                tcp_restart_fin_timeout(tcp);
            }
        }

        /* No longer need to keep track of this packet! */
        tcp->send_unack_num = seq(ohdr) + olen;
        tcp_outbox_remove(tcp, opkt);
        num_acked++;
    }

    /*
     * Update the send window if we think the window field in this
     * packet is "newer" (algorithm blindly copied from RFC793).
     */
    if (cmp(ack(hdr), tcp->send_unack_num) > 0 &&
        (cmp(seq(hdr), tcp->send_wnd_seq) > 0 ||
         (cmp(seq(hdr), tcp->send_wnd_seq) == 0 &&
          cmp(ack(hdr), tcp->send_wnd_ack) >= 0)))
    {
        tcp->send_wnd_size = ntohs(hdr->be_window_size);
        tcp->send_wnd_seq = seq(hdr);
        tcp->send_wnd_ack = ack(hdr);
    }

    /*
     * Restart the retransmission timer if any new data was ACK'd,
     * or stop it if we have no data left to ACK.
     */
    if (list_empty(&tcp->outbox)) {
        tcp_stop_rto_timeout(tcp);
    } else if (num_acked > 0) {
        tcp_restart_rto_timeout(tcp);
    }

    /* If we get three duplicate ACKs, retransmit the earliest packet */
    if (num_acked == 0 && !list_empty(&tcp->outbox)) {
        if (++tcp->num_duplicate_acks == 3) {
            tcp_debugf("Performing fast retransmission of earliest packet\n");
            tcp_pkt_t *txpkt = list_first_entry(&tcp->outbox, tcp_pkt_t, list);
            tcp_outbox_transmit_one(tcp, txpkt);
            tcp->num_duplicate_acks = 0;
        }
    } else {
        tcp->num_duplicate_acks = 0;
    }
}

/*
 * Handles an incoming packet. Adds it to the inbox and updates
 * RCV.NXT. May advance the socket state.
 */
static void
tcp_inbox_handle_rx_skb(tcp_sock_t *tcp, skb_t *skb)
{
    /* Add packet to the inbox if it's not a duplicate */
    if (!tcp_inbox_insert(tcp, skb)) {
        return;
    }

    /*
     * If we get more packets while the FIN timer is active, restart
     * the timeout. In TIME_WAIT state, this means that the remote
     * peer might not have received our ACK for their FIN; in FIN_WAIT_2,
     * this indicates that the remote peer has more packets to send.
     */
    if (tcp_in_state(tcp, TIME_WAIT | FIN_WAIT_2)) {
        tcp_restart_fin_timeout(tcp);
    }

    /* Process packets in the inbox in order until we find a gap */
    list_t *pos, *next;
    list_for_each_safe(pos, next, &tcp->inbox) {
        skb_t *iskb = list_entry(pos, skb_t, list);
        tcp_hdr_t *ihdr = skb_transport_header(iskb);
        int ilen = tcp_seg_len(iskb);

        /* If seq > ack_num, we have a hole, so stop here */
        if (cmp(seq(ihdr), tcp->recv_next_num) > 0) {
            break;
        }

        /*
         * Check if we've already seen this segment before. Note
         * that some segments may overlap, so we check the ending
         * sequence number of the packet.
         */
        if (cmp(seq(ihdr) + ilen, tcp->recv_next_num) <= 0) {
            continue;
        }

        /* Discard any packets after a FIN */
        if (tcp_in_state(tcp, CLOSING | TIME_WAIT | CLOSE_WAIT | LAST_ACK | CLOSED)) {
            tcp_inbox_remove(tcp, iskb);
            continue;
        }

        /* Looks good, advance the ACK number and adjust rwnd to compensate */
        tcp->recv_next_num = seq(ihdr) + ilen;
        tcp->recv_wnd_size -= ilen;

        /* Reached a FIN for the first time */
        if (ihdr->fin) {
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
                tcp_restart_fin_timeout(tcp);
            }
        }
    }

    /*
     * If the socket file is closed, we need to drain the inbox on
     * behalf of userspace.
     */
    if (tcp->read_closed) {
        tcp_inbox_drain(tcp);
    }
}

/*
 * Handles an incoming packet to a connected socket in SYN_SENT state.
 */
static int
tcp_handle_rx_syn_sent(tcp_sock_t *tcp, skb_t *skb)
{
    tcp_hdr_t *hdr = skb_transport_header(skb);

    /*
     * If ACK is present but unacceptable, reset the connection
     * and reply with RST.
     */
    if (hdr->ack &&
        (cmp(ack(hdr), tcp->send_unack_num) < 0 ||
         cmp(ack(hdr), tcp->send_next_num) > 0))
    {
        tcp_debugf("Unacceptable ACK received in SYN_SENT state\n");
        if (!hdr->rst) {
            tcp_reply_rst(net_sock(tcp)->iface, skb);
        }
        tcp->reset = true;
        tcp_set_state(tcp, CLOSED);
        return -1;
    }

    /*
     * If remote requested a reset and the ACK is current,
     * grant their wish. Otherwise, ignore the reset.
     */
    if (hdr->rst) {
        tcp_debugf("Received RST in SYN_SENT state\n");
        if (hdr->ack) {
            tcp->reset = true;
            tcp_set_state(tcp, CLOSED);
        }
        return -1;
    }

    /* Packet seems to be valid, let's handle the SYN now */
    if (hdr->syn) {
        /* Initialize remote sequence number */
        tcp_init_remote_seq(tcp, seq(hdr));

        /* Handle ACK for our SYN */
        if (hdr->ack) {
            tcp_outbox_handle_rx_ack(tcp, hdr);
        }

        /* Add incoming SYN packet to our inbox */
        tcp_inbox_handle_rx_skb(tcp, skb);

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
            tcp_outbox_transmit_one(tcp, syn);
        } else {
            tcp_send_ack(tcp);
        }

        return 0;
    }

    tcp_debugf("Unhandled packet in SYN_SENT state, dropping\n");
    return -1;
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
        return -1;
    }

    /*
     * Handle SYN_SENT state separately, since we don't know
     * the remote peer sequence number yet.
     */
    if (tcp_in_state(tcp, SYN_SENT)) {
        return tcp_handle_rx_syn_sent(tcp, skb);
    }

    /*
     * If the segment is outside of the receive window,
     * discard it and send an ACK if no RST. Note that
     * we still process ACKs, so we don't return immediately.
     */
    bool in_rwnd = tcp_in_rwnd(tcp, seq(hdr), tcp_seg_len(skb));
    if (!in_rwnd) {
        tcp_debugf("Packet outside receive window\n");
    } else {
        /*
         * Handle RST (we use the sequence number instead of
         * ack number here, which is checked above).
         */
        if (hdr->rst) {
            tcp_debugf("Received RST in middle of connection\n");
            tcp->reset = true;
            tcp_set_state(tcp, CLOSED);
            return -1;
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
            return -1;
        }
    }

    /*
     * As per RFC793, if there's no ACK, we drop the segment
     * even if there's data in it.
     */
    if (!hdr->ack) {
        tcp_debugf("No ACK in packet, dropping\n");
        return -1;
    }

    /*
     * Handle invalid ACKs. If we're in the SYN_RECEIVED state,
     * we can only have sent a SYN ourselves, so anything that's
     * outside the window is invalid. According to the spec, we
     * send an ACK if we get an invalid ACK otherwise. For all
     * other states, the ACK could just be stale, so ignore ACKs
     * that are before the window (still reject ones for packets
     * we haven't even sent yet).
     */
    if (tcp_in_state(tcp, SYN_RECEIVED)) {
        if (cmp(ack(hdr), tcp->send_unack_num) < 0 ||
            cmp(ack(hdr), tcp->send_next_num) > 0)
        {
            tcp_debugf("Invalid ACK in SYN_RECEIVED state\n");
            tcp_reply_rst(net_sock(tcp)->iface, skb);
            return -1;
        }
    } else {
        if (cmp(ack(hdr), tcp->send_next_num) > 0) {
            tcp_debugf("Invalid ACK\n");
            tcp_send_ack(tcp);
            return -1;
        }
    }

    /*
     * Handle ACK. This may transmit packets or change the socket
     * state.
     */
    tcp_outbox_handle_rx_ack(tcp, hdr);

    /*
     * Add the incoming packet to our inbox. If the packet has
     * a FIN flag, this will handle it.
     */
    if (in_rwnd && tcp_in_state(tcp, ESTABLISHED | FIN_WAIT_1 | FIN_WAIT_2)) {
        tcp_inbox_handle_rx_skb(tcp, skb);
    }

    /*
     * Send an ACK as long as incoming packet didn't contain RST flag
     * and had some data (wasn't just an empty ACK).
     */
    if (!hdr->rst && tcp_seg_len(skb) > 0) {
        tcp_enqueue_ack(tcp);
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
        /* Reject if backlog is full */
        if (tcp->backlog_capacity == 0) {
            tcp_debugf("Backlog full, dropping connection\n");
            return -1;
        }

        /* Create a new socket */
        net_sock_t *connsock = socket_obj_alloc(SOCK_TCP);
        if (connsock == NULL) {
            tcp_debugf("Failed to allocate socket for incoming connection\n");
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

        tcp_sock_t *conntcp = tcp_sock(connsock);

        /* Initialize remote sequence number */
        tcp_init_remote_seq(conntcp, seq(hdr));

        /* Transition to SYN-received state */
        tcp_set_state(conntcp, SYN_RECEIVED);

        /* Insert SYN packet into inbox */
        tcp_inbox_handle_rx_skb(conntcp, skb);

        /* Reply with SYN-ACK */
        if (tcp_outbox_insert_syn(conntcp) == NULL) {
            /*
             * Note: since socket was created with refcount 0 and
             * the only living refcount is from the TCP state, this
             * will deallocate the socket.
             */
            tcp_set_state(conntcp, CLOSED);
            return -1;
        }
        tcp_outbox_transmit_all(conntcp);

        /* Add socket to backlog for accept() */
        list_add_tail(&conntcp->backlog, &tcp->backlog);
        tcp->backlog_capacity--;
        return 0;
    }

    /* Drop everything else */
    return -1;
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
    tcp_hdr_t *hdr = skb_set_transport_header(skb);
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
        tcp_sock_t *tcp = tcp_acquire(tcp_sock(sock));
        int ret = tcp_handle_rx_connected(tcp, skb);
        tcp_release(tcp);
        return ret;
    }

    /* No connected socket? Okay, try to dispatch to a listening socket */
    sock = get_sock_by_addr(SOCK_TCP, dest_ip, dest_port, ANY_IP, 0);
    if (sock != NULL && sock->listening) {
        tcp_sock_t *tcp = tcp_acquire(tcp_sock(sock));
        int ret = tcp_handle_rx_listening(iface, tcp, skb);
        tcp_release(tcp);
        return ret;
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

    uint32_t seq = urand();
    tcp->sock = sock;
    tcp->state = CLOSED;
    list_init(&tcp->backlog);
    list_init(&tcp->inbox);
    list_init(&tcp->outbox);
    list_init(&tcp->ack_queue);
    timer_init(&tcp->fin_timer);
    timer_init(&tcp->rto_timer);
    tcp->backlog_capacity = 256;
    tcp->recv_wnd_size = TCP_INIT_WND_SIZE;
    tcp->recv_read_num = 0;
    tcp->recv_next_num = 0;
    tcp->send_next_num = seq;
    tcp->send_unack_num = seq;
    tcp->send_wnd_seq = 0;
    tcp->send_wnd_ack = seq;
    tcp->send_wnd_size = TCP_INIT_WND_SIZE;
    tcp->num_duplicate_acks = 0;
    tcp->reset = false;
    tcp->estimated_rtt = -1;
    tcp->variance_rtt = -1;
    tcp->rto = TCP_INIT_RTO_MS;

    sock->private = tcp;
    return 0;
}

/* TCP socket destructor */
void
tcp_dtor(net_sock_t *sock)
{
    tcp_sock_t *tcp = tcp_sock(sock);
    list_t *pos, *next;

    /* Terminate all pending connections */
    if (sock->listening) {
        list_for_each_safe(pos, next, &tcp->backlog) {
            tcp_sock_t *pending = list_entry(pos, tcp_sock_t, backlog);
            tcp_set_state(pending, FIN_WAIT_1);
            if (tcp_outbox_insert_fin(pending) == NULL) {
                /* Note: This will call the pending socket's destructor */
                tcp_set_state(pending, CLOSED);
            } else {
                tcp_outbox_transmit_all(pending);
            }
        }
    } else {
        list_del(&tcp->backlog);
    }

    /* Clear inbox */
    list_for_each_safe(pos, next, &tcp->inbox) {
        skb_t *skb = list_entry(pos, skb_t, list);
        tcp_inbox_remove(tcp, skb);
    }

    /* Clear outbox */
    list_for_each_safe(pos, next, &tcp->outbox) {
        tcp_pkt_t *pkt = list_entry(pos, tcp_pkt_t, list);
        tcp_outbox_remove(tcp, pkt);
    }

    /* Remove from ACK queue */
    list_del(&tcp->ack_queue);

    /* Stop timers */
    timer_cancel(&tcp->fin_timer);
    timer_cancel(&tcp->rto_timer);

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
    int ret;
    tcp_sock_t *tcp = NULL;

    /* Cannot connect already-connected or listening sockets */
    if (sock->connected || sock->listening) {
        return -1;
    }

    /* Socket must be closed at this point */
    tcp = tcp_acquire(tcp_sock(sock));
    assert(tcp_in_state(tcp, CLOSED));

    /* Copy address to kernelspace */
    sock_addr_t tmp;
    if (!copy_from_user(&tmp, addr, sizeof(sock_addr_t))) {
        ret = -1;
        goto exit;
    }

    /* Save original socket state to undo auto-bind */
    bool orig_bound = sock->bound;
    sock_addr_t orig_local_addr = sock->local;
    net_iface_t *orig_iface = sock->iface;

    /* Attempt to connect, auto-binding the socket if needed */
    if (socket_connect_and_bind_addr(sock, tmp.ip, tmp.port) < 0) {
        tcp_debugf("Could not connect socket\n");
        ret = -1;
        goto exit;
    }

    /* Send our SYN packet */
    tcp_set_state(tcp, SYN_SENT);
    if (tcp_outbox_insert_syn(tcp) == NULL) {
        tcp_set_state(tcp, CLOSED);
        ret = -1;
        goto unbind;
    }
    tcp_outbox_transmit_all(tcp);

    ret = 0;

exit:
    if (tcp != NULL) {
        tcp_release(tcp);
    }
    return ret;

unbind:
    sock->connected = false;
    sock->bound = orig_bound;
    sock->iface = orig_iface;
    sock->local = orig_local_addr;
    goto exit;
}

/*
 * listen() socketcall handler. Puts the socket into listening
 * mode. Only works on unconnected sockets.
 */
int
tcp_listen(net_sock_t *sock, int backlog)
{
    int ret;
    tcp_sock_t *tcp = NULL;

    /* Cannot call listen() on a unbound or connected socket */
    if (!sock->bound || sock->connected || backlog <= 0) {
        ret = -1;
        goto exit;
    } else if (sock->listening) {
        ret = 0;
        goto exit;
    }

    /* Socket must be closed at this point */
    tcp = tcp_acquire(tcp_sock(sock));
    assert(tcp_in_state(tcp, CLOSED));

    /* Transition from CLOSED -> LISTEN state */
    sock->listening = true;
    tcp_set_state(tcp, LISTEN);
    tcp->backlog_capacity = backlog;

    ret = 0;

exit:
    if (tcp != NULL) {
        tcp_release(tcp);
    }
    return ret;
}

/*
 * accept() socketcall handler. Accepts a single incoming
 * TCP connection. Copies the remote endpoint's address
 * into addr.
 */
int
tcp_accept(net_sock_t *sock, sock_addr_t *addr)
{
    int ret;
    tcp_sock_t *tcp = NULL;

    /* Cannot call accept() on a non-listening socket */
    if (!sock->listening) {
        ret = -1;
        goto exit;
    }

    /* Check that socket is still open */
    tcp = tcp_acquire(tcp_sock(sock));
    if (!tcp_in_state(tcp, LISTEN)) {
        ret = -1;
        goto exit;
    }

    /* Check if we have anything in the backlog */
    if (list_empty(&tcp->backlog)) {
        ret = -EAGAIN;
        goto exit;
    }

    /* Pop first entry from the backlog */
    tcp_sock_t *conntcp = list_first_entry(&tcp->backlog, tcp_sock_t, backlog);
    net_sock_t *connsock = net_sock(conntcp);

    /* Copy address to userspace */
    if (addr != NULL && !copy_to_user(addr, &connsock->remote, sizeof(sock_addr_t))) {
        ret = -1;
        goto exit;
    }

    /* Bind the socket to a file */
    int fd = socket_obj_bind_file(get_executing_files(), connsock);
    if (fd < 0) {
        ret = -1;
        goto exit;
    }

    /* Consume socket from backlog */
    list_del(&conntcp->backlog);
    conntcp->backlog_capacity++;

    ret = fd;

exit:
    if (tcp != NULL) {
        tcp_release(tcp);
    }
    return ret;
}

/*
 * recvfrom() socketcall handler. Reads the specified number of
 * bytes from the remote endpoint. addr is ignored.
 */
int
tcp_recvfrom(net_sock_t *sock, void *buf, int nbytes, sock_addr_t *addr)
{
    int ret;
    tcp_sock_t *tcp = NULL;

    /* Standard error checks */
    if (nbytes < 0 || !sock->connected) {
        ret = -1;
        goto exit;
    }

    /*
     * If the socket is closed due to an error (reset),
     * reading from it is a failure. If it's closed but under
     * normal conditions, let the user keep reading from the
     * socket (this can occur if user calls shutdown() followed
     * by read()).
     */
    tcp = tcp_acquire(tcp_sock(sock));
    if (tcp_in_state(tcp, CLOSED) && tcp->reset) {
        ret = -1;
        goto exit;
    } else if (tcp_in_state(tcp, SYN_SENT | SYN_RECEIVED)) {
        ret = -EAGAIN;
        goto exit;
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
        if (cmp(seq(hdr), tcp->recv_next_num) > 0) {
            break;
        }

        /* Find starting byte, based on how much we've already read */
        int offset = (int)(tcp->recv_read_num - seq(hdr));
        int bytes_remaining = tcp_body_len(skb) - offset;
        if (bytes_remaining >= 0) {
            /* Clamp to actual size of buffer */
            int bytes_to_copy = bytes_remaining;
            if (bytes_to_copy > nbytes - copied) {
                bytes_to_copy = nbytes - copied;
            }

            /* Now do the copy, only return -1 if no bytes could be copied */
            uint8_t *body = skb_data(skb);
            uint8_t *start = &body[offset];
            if (!copy_to_user(&bufp[copied], start, bytes_to_copy)) {
                if (copied == 0) {
                    ret = -1;
                } else {
                    ret = copied;
                }
                goto exit;
            }
            tcp->recv_read_num += bytes_to_copy;
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

        /* We're done with this packet, remove it and expand rwnd */
        tcp_inbox_done(tcp, skb);
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
            ret = 0;
        } else {
            ret = -EAGAIN;
        }
        goto exit;
    }

    ret = copied;

exit:
    if (tcp != NULL) {
        tcp_release(tcp);
    }
    return ret;
}

/*
 * sendto() socketcall handler. Splits the input buffer into
 * TCP packets and sends them to the remote endpoint. Fails if
 * the writing end of the socket is closed. addr is ignored.
 */
int
tcp_sendto(net_sock_t *sock, const void *buf, int nbytes, const sock_addr_t *addr)
{
    int ret;
    tcp_sock_t *tcp = NULL;

    /* Standard error checks */
    if (nbytes < 0 || !sock->connected) {
        ret = -1;
        goto exit;
    }

    /* Check that socket write end is still open */
    tcp = tcp_acquire(tcp_sock(sock));
    if (tcp_in_state(tcp, CLOSED | FIN_WAIT_1 | FIN_WAIT_2 | CLOSING | TIME_WAIT)) {
        ret = -1;
        goto exit;
    } else if (tcp_in_state(tcp, SYN_SENT | SYN_RECEIVED)) {
        ret = -EAGAIN;
        goto exit;
    }

    /* Limit number of bytes to remaining send window */
    int outbox_used = (int)(tcp->send_next_num - tcp->send_unack_num);
    int outbox_free = (int)tcp->send_wnd_size - outbox_used;
    if (outbox_free <= 0) {
        ret = -EAGAIN;
        goto exit;
    }

    if (nbytes > outbox_free) {
        nbytes = outbox_free;
    } else if (nbytes == 0) {
        ret = 0;
        goto exit;
    }

    /* Copy data from userspace into TCP outbox */
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
        hdr->be_seq_num = htonl(tcp->send_next_num);

        /* Insert packet into outbox */
        tcp_pkt_t *pkt = tcp_outbox_insert(tcp, skb);
        if (pkt == NULL) {
            skb_release(skb);
            break;
        }

        skb_release(skb);
        sent += body_len;
    }

    /* Transmit new packets immediately if within send window */
    tcp_outbox_transmit_all(tcp);

    /*
     * No bytes sent indicates complete failure;
     * 0 < sent < nbytes indicates partial failure.
     */
    if (sent == 0) {
        ret = -1;
    } else {
        ret = sent;
    }

exit:
    if (tcp != NULL) {
        tcp_release(tcp);
    }
    return ret;
}

/*
 * shutdown() socketcall handler. Sends a FIN to the
 * remote endpoint and closes the writing end of the socket.
 */
int
tcp_shutdown(net_sock_t *sock)
{
    int ret;
    tcp_sock_t *tcp = NULL;

    if (!sock->connected) {
        tcp_debugf("shutdown() called on an unconnected socket\n");
        ret = -1;
        goto exit;
    }

    tcp = tcp_acquire(tcp_sock(sock));
    tcp_close_write(tcp);

    ret = 0;

exit:
    if (tcp != NULL) {
        tcp_release(tcp);
    }
    return ret;
}

/*
 * close() socketcall handler. Sends a FIN to the
 * remote endpoint and closes the writing end of the
 * socket. The socket will be inaccessible from userspace,
 * but will remain alive in the kernel until the FIN
 * has been ACK'd.
 */
void
tcp_close(net_sock_t *sock)
{
    tcp_sock_t *tcp = tcp_acquire(tcp_sock(sock));
    tcp->read_closed = true;
    tcp_inbox_drain(tcp);
    tcp_close_write(tcp);
    tcp_release(tcp);
}
