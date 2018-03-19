#ifndef _NET_H
#define _NET_H

#include "lib.h"
#include "skb.h"

#ifndef ASM

#define bswap16(x) (\
    ((uint16_t)(x) & 0x00ff) << 8 |\
    ((uint16_t)(x) & 0xff00) >> 8)

#define bswap32(x) (\
    ((uint32_t)(x) & 0x000000ff) << 24 |\
    ((uint32_t)(x) & 0x0000ff00) << 8  |\
    ((uint32_t)(x) & 0x00ff0000) >> 8  |\
    ((uint32_t)(x) & 0xff000000) >> 24)

#define ntohs(x) bswap16(x)
#define htons(x) bswap16(x)
#define ntohl(x) bswap32(x)
#define htonl(x) bswap32(x)

typedef struct { uint8_t bytes[4]; } ip_addr_t;
typedef struct { uint8_t bytes[6]; } mac_addr_t;

#define IP(a, b, c, d) ((ip_addr_t){.bytes = {(a), (b), (c), (d)}})
#define MAC(a, b, c, d, e, f) ((mac_addr_t){.bytes = {(a), (b), (c), (d), (e), (f)}})

#define MAC_BROADCAST MAC(0xff, 0xff, 0xff, 0xff, 0xff, 0xff)

#define ip_equals(a, b) (memcmp((a).bytes, (b).bytes, 4) == 0)
#define mac_equals(a, b) (memcmp((a).bytes, (b).bytes, 6) == 0)
#define iptoh(ip) (*(uint32_t *)&(ip).bytes)
#define ipton(ip) (htonl(iptoh(ip)))

/* Ugly forward declaration b/c of circular reference w/ arp.h */
typedef struct arp_cache_t arp_cache_t;

/* Layer-2 Ethernet device */
typedef struct net_dev_t {
    char name[32];
    mac_addr_t mac_addr;
    int (*send_mac_skb)(struct net_dev_t *dev, skb_t *skb);
} net_dev_t;

/* Layer-3 IP interface */
typedef struct net_iface_t {
    char name[32];
    ip_addr_t subnet_mask;
    ip_addr_t ip_addr;
    net_dev_t *dev;
    int (*send_ip_skb)(struct net_iface_t *iface, skb_t *skb, ip_addr_t addr);
} net_iface_t;

typedef union {
    struct {
        mac_addr_t addr;
        uint16_t type;
    } mac;
    struct {
        uint16_t id;
        uint8_t ttl;
        uint8_t proto;
        ip_addr_t addr;
    } ip;
    struct {
        ip_addr_t addr;
        uint16_t port;
    } udp;
} sockargs_t;

/* Finds the interface for the given device */
net_iface_t *net_get_interface(net_dev_t *dev);

/* Finds the interface that routes to the given IP */
net_iface_t *net_route(ip_addr_t *ip);

/* Registers a new interface */
void net_register_interface(net_iface_t *iface);

#endif /* ASM */

#endif /* _NET_H */
