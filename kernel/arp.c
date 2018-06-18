#include "arp.h"
#include "lib.h"
#include "debug.h"
#include "list.h"
#include "myalloc.h"
#include "ethernet.h"
#include "rtc.h"

#define ARP_OP_REQUEST 1
#define ARP_OP_REPLY 2
#define ARP_HWTYPE_ETHERNET 1
#define ARP_PROTOTYPE_IPV4 0x0800

/*
 * Timeout in seconds for the entries in the ARP
 * cache. Resolve timeout = how long to wait for a
 * reply before declaring it unreachable. Cache
 * timeout = how long to cache results before sending
 * a new ARP request.
 */
#define ARP_RESOLVE_TIMEOUT (1 * RTC_HZ)
#define ARP_CACHE_TIMEOUT (60 * RTC_HZ)

/* ARP cache entry */
typedef struct {
    ip_addr_t ip_addr;
    mac_addr_t mac_addr;
    bool exists : 1;
    bool resolved : 1;
    int timestamp;
} arp_entry_t;

/* ARP packet header */
typedef struct {
    uint16_t be_hw_type;
    uint16_t be_proto_type;
    uint8_t hw_len;
    uint8_t proto_len;
    uint16_t be_op;
} __packed arp_hdr_t;

/* ARP IPv4 <-> Ethernet packet body */
typedef struct {
    mac_addr_t src_hw_addr;
    ip_addr_t src_proto_addr;
    mac_addr_t dest_hw_addr;
    ip_addr_t dest_proto_addr;
} __packed arp_body_t;

/*
 * Structure for packets that need to be sent, and the IP address
 * they're waiting on.
 */
typedef struct {
    list_t list;
    skb_t *skb;
    net_dev_t *dev;
    ip_addr_t ip;
} queue_pkt_t;

/* Queue for packets waiting for an ARP reply */
static list_declare(packet_queue);

/*
 * We only have one Ethernet device, so a global ARP
 * cache is sufficient for now. If we ever decide to
 * support multiple devices, we'll need a hashing
 * mechanism to separate the entries by device.
 *
 * Since we're running in QEMU, there's only like
 * 4 or so destinations that we need to know about.
 * The rest will go through the default gateway.
 * Maintain some excess space for bogus entries.
 */
static arp_entry_t arp_cache[16];

/*
 * Checks whether an ARP entry is expired; that is,
 * its data is too old and a new ARP request needs to
 * be sent.
 */
static bool
arp_entry_is_expired(arp_entry_t *entry)
{
    return entry->timestamp + ARP_CACHE_TIMEOUT < rtc_get_counter();
}

/*
 * Checks whether an ARP entry is unreachable; that is,
 * it has not been resolved and the reply timeout
 * has been reached. These are not immediately deleted
 * from the cache since we want to prevent the same
 * IP address from being re-requested for a short time.
 */
static bool
arp_entry_is_unreachable(arp_entry_t *entry)
{
    if (entry->resolved) return false;
    return entry->timestamp + ARP_RESOLVE_TIMEOUT < rtc_get_counter();
}

/*
 * Flushes all packets waiting for an ARP reply from the
 * specified IP address. If mac is not null, the packets
 * are sent. If it is null, they are dropped.
 */
static void
arp_queue_flush(ip_addr_t ip, const mac_addr_t *mac)
{
    list_t *pos, *next;
    list_for_each_safe(pos, next, &packet_queue) {
        queue_pkt_t *pkt = list_entry(pos, queue_pkt_t, list);
        if (ip_equals(pkt->ip, ip)) {
            if (mac != NULL) {
                ethernet_send_mac(pkt->dev, pkt->skb, *mac, ETHERTYPE_IPV4);
            }
            skb_release(pkt->skb);
            list_del(&pkt->list);
            free(pkt);
        }
    }
}

/*
 * Enqueues an IP packet to be sent when the corresponding MAC
 * address is known. This will return 0 if an ARP packet
 * was sent but the result is not yet known. This does not
 * indicate that the packet will actually be successfully
 * transmitted - if no reply is received, the packet will
 * be dropped.
 */
int
arp_queue_insert(net_dev_t *dev, skb_t *skb, ip_addr_t ip)
{
    queue_pkt_t *pkt = malloc(sizeof(queue_pkt_t));
    if (pkt == NULL) {
        debugf("Cannot allocate space for packet\n");
        return -1;
    }

    pkt->dev = dev;
    pkt->skb = skb_retain(skb);
    pkt->ip = ip;
    list_add_tail(&pkt->list, &packet_queue);
    return 0;
}

/*
 * Inserts an entry into the ARP cache. If there is an
 * existing entry with the given IP address, this will
 * overwrite it. The MAC address may be null to indicate
 * that we do not know the mapping result. Returns 0 on
 * success, < 0 on failure (cache is full).
 */
static int
arp_cache_insert(ip_addr_t ip, const mac_addr_t *mac)
{
    /*
     * Find existing entry, or the first blank entry.
     * Existing entry has higher priority.
     */
    arp_entry_t *entry = NULL;
    int i;
    for (i = 0; i < array_len(arp_cache); ++i) {
        arp_entry_t *tmp = &arp_cache[i];

        /* While we're here, clean up expired entries */
        if (tmp->exists && arp_entry_is_expired(tmp)) {
            arp_queue_flush(tmp->ip_addr, NULL);
            tmp->exists = false;
        }

        /* Pick a free slot if available */
        if (!tmp->exists && entry == NULL) {
            entry = tmp;
        }

        /* Stop if the address is already in the cache */
        if (tmp->exists && ip_equals(ip, tmp->ip_addr)) {
            entry = tmp;
            break;
        }
    }

    /* No free space and mapping doesn't already exist */
    if (entry == NULL) {
        debugf("ARP cache full, cannot insert entry\n");
        return -1;
    }

    /* Update entry in the cache */
    entry->exists = true;
    entry->timestamp = rtc_get_counter();
    entry->ip_addr = ip;
    if (mac != NULL) {
        entry->mac_addr = *mac;
        entry->resolved = true;
    } else {
        entry->resolved = false;
    }
    return 0;
}

/*
 * Attempts to resolve an IP address to a MAC address,
 * using only the ARP cache.
 *
 * ARP_INVALID: MAC address is not cached and no request
 * has been sent yet
 *
 * ARP_UNREACHABLE: timed out waiting for an ARP reply
 *
 * ARP_WAITING: ARP request sent, no reply received yet
 *
 * ARP_REACHABLE: MAC address is cached and not stale
 */
arp_state_t
arp_get_state(net_dev_t *dev, ip_addr_t ip, mac_addr_t *mac)
{
    int i;
    for (i = 0; i < array_len(arp_cache); ++i) {
        arp_entry_t *entry = &arp_cache[i];
        if (entry->exists && ip_equals(ip, entry->ip_addr)) {
            if (arp_entry_is_expired(entry)) {
                arp_queue_flush(entry->ip_addr, NULL);
                entry->exists = false;
                return ARP_INVALID;
            } else if (arp_entry_is_unreachable(entry)) {
                return ARP_UNREACHABLE;
            } else if (!entry->resolved) {
                return ARP_WAITING;
            } else {
                *mac = entry->mac_addr;
                return ARP_REACHABLE;
            }
        }
    }
    return ARP_INVALID;
}

/*
 * Sends an ARP packet. iface determines which device to send
 * the packet on; op can either be a request or a reply.
 * ip and mac are the destination addresses (mac can be
 * BROADCAST_MAC if not known).
 */
static int
arp_send(net_iface_t *iface, ip_addr_t ip, mac_addr_t mac, int op)
{
    /* Allocate new SKB */
    int hdr_len = sizeof(arp_hdr_t) + sizeof(ethernet_hdr_t);
    skb_t *skb = skb_alloc(sizeof(arp_body_t) + hdr_len);
    if (skb == NULL) {
        debugf("Failed to allocate new SKB\n");
        return -1;
    }
    skb_reserve(skb, hdr_len);

    /* Fill out ARP body */
    arp_body_t *body = skb_put(skb, sizeof(arp_body_t));
    body->src_hw_addr = iface->dev->mac_addr;
    body->src_proto_addr = iface->ip_addr;
    body->dest_hw_addr = mac;
    body->dest_proto_addr = ip;

    /* Fill out ARP header */
    arp_hdr_t *hdr = skb_push(skb, sizeof(arp_hdr_t));
    hdr->be_hw_type = htons(ARP_HWTYPE_ETHERNET);
    hdr->be_proto_type = htons(ARP_PROTOTYPE_IPV4);
    hdr->hw_len = sizeof(mac_addr_t);
    hdr->proto_len = sizeof(ip_addr_t);
    hdr->be_op = htons(op);

    /* Send out the packet */
    int ret = ethernet_send_mac(iface->dev, skb, mac, ETHERTYPE_ARP);
    skb_release(skb);
    return ret;
}

/*
 * Sends an ARP request to the specified IP address.
 */
int
arp_send_request(net_iface_t *iface, ip_addr_t ip)
{
    /* Insert pending entry into ARP cache */
    if (arp_cache_insert(ip, NULL) < 0) {
        return -1;
    }

    /* Send ARP request */
    return arp_send(iface, ip, BROADCAST_MAC, ARP_OP_REQUEST);
}

/*
 * Sends an ARP reply to the specified IP/MAC address.
 */
static int
arp_send_reply(net_iface_t *iface, ip_addr_t ip, mac_addr_t mac)
{
    /* Insert into cache (we always send the reply, so ignore failure) */
    arp_cache_insert(ip, &mac);

    /* Send ARP reply */
    return arp_send(iface, ip, mac, ARP_OP_REPLY);
}

/*
 * Handles an ARP reply packet. Inserts the reply into the
 * ARP cache for the device that received the packet, then
 * sends all enqueued packets for the corresponding IP.
 */
static int
arp_handle_reply(net_dev_t *dev, skb_t *skb)
{
    arp_body_t *body = skb_data(skb);
    int ret = arp_cache_insert(body->src_proto_addr, &body->src_hw_addr);
    arp_queue_flush(body->src_proto_addr, &body->src_hw_addr);
    return ret;
}

/*
 * Handles an ARP request packet. Replies with the MAC address
 * corresponding to the requested IP address, if it matches
 * the interface.
 */
static int
arp_handle_request(net_dev_t *dev, skb_t *skb)
{
    /* Determine interface that packet arrived on */
    net_iface_t *iface = net_get_interface(dev);
    if (iface == NULL) {
        return -1;
    }

    /* Check that dest IP addr equals interface's IP addr */
    arp_body_t *body = skb_data(skb);
    if (!ip_equals(iface->ip_addr, body->dest_proto_addr)) {
        return -1;
    }

    /* Okay, send our reply */
    return arp_send_reply(iface, body->src_proto_addr, body->src_hw_addr);
}

/*
 * Handles an ARP packet, updating the ARP cache. Currently
 * this only handles replies, thought it would probably be
 * a lot more efficient to also cache requests.
 */
int
arp_handle_rx(net_dev_t *dev, skb_t *skb)
{
    /* Check packet size */
    if (!skb_may_pull(skb, sizeof(arp_hdr_t) + sizeof(arp_body_t))) {
        debugf("ARP packet too small\n");
        return -1;
    }

    /* Pop ARP header */
    arp_hdr_t *hdr = skb_reset_network_header(skb);
    skb_pull(skb, sizeof(arp_hdr_t));

    /* Ensure we have an Ethernet <-> IPv4 ARP packet */
    if (ntohs(hdr->be_hw_type) != ARP_HWTYPE_ETHERNET)
        return -1;
    if (ntohs(hdr->be_proto_type) != ARP_PROTOTYPE_IPV4)
        return -1;
    if (hdr->hw_len != sizeof(mac_addr_t))
        return -1;
    if (hdr->proto_len != sizeof(ip_addr_t))
        return -1;

    /* Handle op accordingly */
    switch (ntohs(hdr->be_op)) {
    case ARP_OP_REPLY:
        return arp_handle_reply(dev, skb);
    case ARP_OP_REQUEST:
        return arp_handle_request(dev, skb);
    default:
        debugf("Unknown ARP op value\n");
        return -1;
    }
}
