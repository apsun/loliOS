#include "ip.h"
#include "debug.h"
#include "string.h"
#include "tcp.h"
#include "udp.h"

/*
 * Computes a IPv4, TCP, or UDP checksum. The sum should
 * be computed from calling ip_partial_checksum().
 */
static uint16_t
ip_checksum(uint32_t sum)
{
    while (sum & ~0xffff) {
        sum = (sum & 0xffff) + (sum >> 16);
    }
    return ~sum & 0xffff;
}

/*
 * Performs a partial checksum. Pass the sum of the
 * partial results to ip_checksum() to get the final
 * result.
 */
static uint32_t
ip_partial_checksum(const void *buf, int len)
{
    /*
     * WARNING: Do not iterate the buffer in words, as it causes
     * undefined behavior in -O2 when this function is inlined.
     * Slow and steady wins the race here. See char aliasing rules.
     */
    const uint8_t *bufp = buf;
    uint32_t sum = 0;
    int i;
    for (i = 0; i < (len & ~1); i += 2) {
        sum += bufp[i] << 8 | bufp[i + 1];
    }
    if (len & 1) {
        sum += bufp[len - 1] << 8;
    }
    return sum;
}

/*
 * Computes a TCP/UDP checksum. The SKB should contain the
 * transport header already, with the checksum set to zero. The
 * source IP is the address of the interface that will be
 * used to send the datagram.
 */
uint16_t
ip_pseudo_checksum(skb_t *skb, ip_addr_t src_ip, ip_addr_t dest_ip, int protocol)
{
    ip_pseudo_hdr_t phdr;
    phdr.src_ip = src_ip;
    phdr.dest_ip = dest_ip;
    phdr.zero = 0;
    phdr.protocol = protocol;
    phdr.be_length = htons(skb_len(skb));
    uint32_t phdr_sum = ip_partial_checksum(&phdr, sizeof(phdr));
    uint32_t skb_sum = ip_partial_checksum(skb_data(skb), skb_len(skb));
    uint16_t sum = ip_checksum(phdr_sum + skb_sum);
    if (sum == 0) {
        sum = 0xffff;
    }
    return sum;
}

/*
 * Checks whether the IP/TCP/UDP checksum for the
 * given header is valid.
 */
static bool
ip_verify_checksum(const void *buf, int len)
{
    return ip_checksum(ip_partial_checksum(buf, len)) == 0;
}

/*
 * Handles an incoming IP packet.
 */
int
ip_handle_rx(net_iface_t *iface, skb_t *skb)
{
    /* Possible that net_get_interface() returns null */
    if (iface == NULL) {
        debugf("No interface for packet\n");
        return -1;
    }

    /* Check packet size */
    if (!skb_may_pull(skb, sizeof(ip_hdr_t))) {
        debugf("IP packet too small\n");
        return -1;
    }

    /* Pop IP header, trim off Ethernet padding */
    ip_hdr_t *hdr = skb_reset_network_header(skb);
    uint16_t ip_len = ntohs(hdr->be_total_length);
    if (ip_len < sizeof(ip_hdr_t) || ip_len > skb_len(skb)) {
        debugf("Invalid packet length\n");
        return -1;
    }
    skb_trim(skb, ip_len);
    skb_pull(skb, sizeof(ip_hdr_t));

    /* Drop fragmented packets */
    if (ntohs(hdr->be_flags) & 0xffbf) {
        debugf("Fragmented packets not supported\n");
        return -1;
    }

    /* Check if we accidentally got someone else's packet */
    if (!ip_equals(hdr->dest_ip, iface->ip_addr)) {
        debugf("Destination IP mismatch\n");
        return -1;
    }

    /* Verify checksum */
    if (!ip_verify_checksum(hdr, sizeof(ip_hdr_t))) {
        debugf("Invalid IP header checksum\n");
        return -1;
    }

    /* Forward to upper layers */
    switch (hdr->protocol) {
    case IPPROTO_TCP:
        return tcp_handle_rx(iface, skb);
    case IPPROTO_UDP:
        return udp_handle_rx(iface, skb);
    default:
        debugf("Unhandled IP protocol\n");
        return -1;
    }
}

/*
 * Sends an IP packet to the specified IP address.
 * iface is the interface to send the packet on,
 * neigh_ip is the next-hop address (equal to
 * dest_ip if the destination is in the same subnet,
 * or the gateway otherwise).
 */
int
ip_send(net_iface_t *iface, ip_addr_t neigh_ip, skb_t *skb, ip_addr_t dest_ip, int protocol)
{
    assert(skb_network_header(skb) == NULL);
    ip_hdr_t *hdr = skb_push(skb, sizeof(ip_hdr_t));

    /* Fill out IP header */
    hdr->ihl = sizeof(ip_hdr_t) / 4;
    hdr->version = 4;
    hdr->tos = 0;
    hdr->be_total_length = htons(skb_len(skb));
    hdr->be_identification = htons(0);
    hdr->be_flags = htons(0);
    hdr->ttl = 64;
    hdr->protocol = protocol;
    hdr->be_checksum = htons(0);
    hdr->src_ip = iface->ip_addr;
    hdr->dest_ip = dest_ip;
    hdr->be_checksum = htons(0);
    hdr->be_checksum = htons(ip_checksum(ip_partial_checksum(hdr, sizeof(ip_hdr_t))));

    /* Forward to interface's IP packet handler */
    int ret = iface->send_ip_skb(iface, skb, neigh_ip);
    skb_pull(skb, sizeof(ip_hdr_t));
    return ret;
}
