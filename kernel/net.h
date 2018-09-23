#ifndef _NET_H
#define _NET_H

#include "types.h"
#include "lib.h"
#include "skb.h"

#ifndef ASM

/* Type-safe network endianness wrappers */
typedef struct { uint32_t raw; } be32_t;
typedef struct { uint16_t raw; } be16_t;

/* Network endianness swapping macros */
#define ntohs(x) bswap16((x).raw)
#define htons(x) ((be16_t){.raw = bswap16(x)})
#define ntohl(x) bswap32((x).raw)
#define htonl(x) ((be32_t){.raw = bswap32(x)})

/* Addresses and address accessories */
typedef struct { uint8_t bytes[4]; } ip_addr_t;
typedef struct { uint8_t bytes[6]; } mac_addr_t;

/* Convenience macros for creating addresses */
#define IP(a, b, c, d) ((ip_addr_t){.bytes = {(a), (b), (c), (d)}})
#define MAC(a, b, c, d, e, f) ((mac_addr_t){.bytes = {(a), (b), (c), (d), (e), (f)}})

/* Commonly used addresses */
#define INVALID_IP    IP(0, 0, 0, 0)
#define ANY_IP        IP(0, 0, 0, 0)
#define BROADCAST_IP  IP(255, 255, 255, 255)
#define BROADCAST_MAC MAC(0xff, 0xff, 0xff, 0xff, 0xff, 0xff)

/* "==" for address types */
#define ip_equals(a, b) (memcmp((a).bytes, (b).bytes, 4) == 0)
#define mac_equals(a, b) (memcmp((a).bytes, (b).bytes, 6) == 0)

/* ip_addr_t to uint32_t (type punning breaks when optimizations are enabled) */
#define iptoh(ip) (\
    ((ip).bytes[0] << 0)  |\
    ((ip).bytes[1] << 8)  |\
    ((ip).bytes[2] << 16) |\
    ((ip).bytes[3] << 24))
#define ipton(ip) (htonl(iptoh(ip)))

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
    ip_addr_t gateway_addr;
    net_dev_t *dev;
    int (*send_ip_skb)(struct net_iface_t *iface, skb_t *skb, ip_addr_t addr);
} net_iface_t;

/* Finds the interface for the given device */
net_iface_t *net_get_interface(net_dev_t *dev);

/* Finds the interface with the given local IP */
net_iface_t *net_find(ip_addr_t ip);

/* Finds the interface that routes to the given IP */
net_iface_t *net_route(net_iface_t *iface, ip_addr_t ip, ip_addr_t *neigh_ip);

/* Registers a new interface */
void net_register_interface(net_iface_t *iface);

#endif /* ASM */

#endif /* _NET_H */
