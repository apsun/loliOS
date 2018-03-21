#ifndef _SKB_H
#define _SKB_H

#include "types.h"
#include "lib.h"

#ifndef ASM

/*
 * Socket kernel buffer structure, just like the one in Linux.
 */
typedef struct {
    uint16_t refcnt; /* Must be first 2B, to ensure IP header alignment */
    uint8_t buf[1514]; /* Maximum size of Ethernet frame */
    uint8_t *head;
    uint8_t *data;
    uint8_t *tail;
    uint8_t *end;
    int len;
    void *mac_header;
    void *network_header;
    void *transport_header;
} skb_t;

/* Allocates a new SKB */
skb_t *skb_alloc(void);

/* Increments the SKB reference count */
skb_t *skb_retain(skb_t *skb);

/* Decrements the SKB reference count and frees it if zero */
void skb_release(skb_t *skb);

/* Returns a pointer to the beginning of the data section */
void *skb_data(skb_t *skb);

/* Returns the data length */
int skb_len(skb_t *skb);

/* Returns the amount of space at the start of the SKB */
int skb_headroom(skb_t *skb);

/* Returns the amount of space at the end of the SKB */
int skb_tailroom(skb_t *skb);

/* Pushes data at the start of the data section */
void *skb_push(skb_t *skb, int len);

/* Returns whether the specified number of bytes can be pulled */
bool skb_may_pull(skb_t *skb, int len);

/* Pops data at the start of the data section */
void *skb_pull(skb_t *skb, int len);

/* Appends data to the end of the data section */
void *skb_put(skb_t *skb, int len);

/* Removes data from the end of the data section */
void skb_trim(skb_t *skb, int len);

/* Reserves space for the head section */
void skb_reserve(skb_t *skb, int len);

/* Set the location of the headers to the start of the data section */
void *skb_reset_mac_header(skb_t *skb);
void *skb_reset_network_header(skb_t *skb);
void *skb_reset_transport_header(skb_t *skb);

/* Returns the headers set by the skb_reset_*_header() functions */
void *skb_mac_header(skb_t *skb);
void *skb_network_header(skb_t *skb);
void *skb_transport_header(skb_t *skb);

#endif /* ASM */

#endif /* _SKB_H */
