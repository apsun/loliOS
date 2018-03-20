#include "ip.h"
#include "lib.h"
#include "debug.h"
#include "udp.h"

/*
 * Computes the IPv4 header checksum for the given
 * packet. The checksum field must be set to 0.
 */
static uint16_t
ip_compute_checksum(ip_hdr_t *hdr)
{
    ASSERT((sizeof(*hdr) & 1) == 0);
    uint32_t sum = 0;
    uint16_t *start = (uint16_t *)hdr;
    uint16_t *end = (uint16_t *)(hdr + 1);
    uint16_t *curr;
    for (curr = start; curr < end; ++curr) {
        sum += ntohs(*curr);
        sum = (sum & 0xffff) + (sum >> 16);
    }
    return ~sum;
}

/*
 * Checks whether the IPv4 header checksum for the
 * given packet is valid.
 */
static bool
ip_verify_checksum(ip_hdr_t *hdr)
{
    return ip_compute_checksum(hdr) == 0;
}

/*
 * Handles an incoming IP packet.
 */
int
ip_handle_rx(net_iface_t *iface, skb_t *skb)
{
    /* Check packet size */
    if (!skb_may_pull(skb, sizeof(ip_hdr_t))) {
        debugf("IP packet too small\n");
        return -1;
    }

    /* Pop IP header, trim off Ethernet padding */
    ip_hdr_t *hdr = skb_reset_mac_header(skb);
    if (ntohs(hdr->be_total_length) < sizeof(ip_hdr_t)) {
        debugf("Invalid packet length\n");
        return -1;
    }
    skb_trim(skb, ntohs(hdr->be_total_length));
    skb_pull(skb, sizeof(ip_hdr_t));

    /* Drop packets with unhandled fields */
    if (hdr->ecn_dscp != 0) {
        debugf("ECN/DSCP not supported\n");
        return -1;
    } else if (ntohs(hdr->be_flags) & 0xffbf) {
        debugf("Fragmented packets not supported\n");
        return -1;
    }

    /* Verify checksum */
    if (!ip_verify_checksum(hdr)) {
        debugf("Invalid IP header checksum\n");
        return -1;
    }

    /* Route packet to socket layer */
    switch (hdr->protocol) {
    case IPPROTO_ICMP:
        debugf("Received ICMP packet\n");
        return -1;
    case IPPROTO_TCP:
        debugf("Received TCP packet\n");
        return -1;
    case IPPROTO_UDP:
        return udp_handle_rx(iface, skb);
    default:
        debugf("Unhandled IP protocol\n");
        return -1;
    }
}

/*
 * Sends an IP packet to the specified IP address.
 * The interface may be null; if provided, then the
 * destination IP must be in the same subnet as the
 * interface.
 */
int
ip_send(net_iface_t *iface, skb_t *skb, ip_addr_t ip, int protocol)
{
    /* Determine interface and IP address for sending packet */
    ip_addr_t neigh_ip = ip;
    if (iface == NULL) {
        iface = net_route(&neigh_ip);
        if (iface == NULL) {
            debugf("No interface to handle IP packet\n");
            return -1;
        }
    }

    /* Prepend IP header */
    ip_hdr_t *hdr = skb_push(skb, sizeof(ip_hdr_t));
    hdr->ihl = sizeof(ip_hdr_t) / 4;
    hdr->version = 4;
    hdr->ecn_dscp = 0;
    hdr->be_total_length = htons(skb_len(skb));
    hdr->be_identification = htons(0);
    hdr->be_flags = htons(0);
    hdr->ttl = 64;
    hdr->protocol = protocol;
    hdr->be_checksum = htons(0);
    hdr->src_ip = iface->ip_addr;
    hdr->dest_ip = ip;
    hdr->be_checksum = htons(ip_compute_checksum(hdr));

    /* Forward to interface's IP packet handler */
    return iface->send_ip_skb(iface, skb, neigh_ip);
}
