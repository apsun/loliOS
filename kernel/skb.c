#include "skb.h"
#include "lib.h"
#include "debug.h"
#include "myalloc.h"

/* 1500 bytes for Ethernet body + 14 byte Ethernet header */
#define SKB_MAX_LEN 1514

/*
 * Allocates and initializes a new SKB. Returns NULL if
 * there are no free SKBs. The new SKB has reference count
 * initially set to 1.
 */
skb_t *
skb_alloc(int size)
{
    assert(size >= 0 && size <= SKB_MAX_LEN);
    skb_t *skb = malloc(sizeof(skb_t) + size);
    if (skb == NULL) {
        return NULL;
    }

    skb->refcnt = 1;
    skb->len = 0;
    skb->head = &skb->buf[0];
    skb->data = &skb->buf[0];
    skb->tail = &skb->buf[0];
    skb->end = &skb->buf[size];
    skb->mac_header = NULL;
    skb->network_header = NULL;
    skb->transport_header = NULL;
    return skb;
}

/*
 * Increments the reference count of a SKB. You should
 * have a corresponding call to skb_release() to decrement
 * the reference count.
 */
skb_t *
skb_retain(skb_t *skb)
{
    assert(skb->refcnt > 0);
    skb->refcnt++;
    return skb;
}

/*
 * Decrements the reference count of a SKB. If the reference
 * count reaches zero, the SKB is deallocated.
 */
void
skb_release(skb_t *skb)
{
    assert(skb->refcnt > 0);
    if (--skb->refcnt == 0) {
        free(skb);
    }
}

/*
 * Returns a pointer to the beginning of the data section.
 */
void *
skb_data(skb_t *skb)
{
    assert(skb->refcnt > 0);
    return skb->data;
}

/*
 * Returns the length of the data in the buffer.
 */
int
skb_len(skb_t *skb)
{
    assert(skb->refcnt > 0);
    return skb->len;
}

/*
 * Returns the length in bytes that the buffer's data section
 * may still be expanded by at the start.
 */
int
skb_headroom(skb_t *skb)
{
    assert(skb->refcnt > 0);
    return skb->data - skb->head;
}

/*
 * Returns the length in bytes that the buffer's data section
 * may still be expanded by at the end.
 */
int
skb_tailroom(skb_t *skb)
{
    assert(skb->refcnt > 0);
    return skb->end - skb->tail;
}

/*
 * Pushes data into the SKB at the beginning of the data
 * section. Panics if there is not enough space in the
 * head section to cover the allocation. Returns a pointer
 * to the *new* beginning of the data section.
 */
void *
skb_push(skb_t *skb, int len)
{
    assert(skb->refcnt > 0);
    assert(skb->data - len >= skb->head);
    skb->data -= len;
    skb->len += len;
    return skb->data;
}

/*
 * Checks whether there is enough data in the data section
 * to pull the specified number of bytes.
 */
bool
skb_may_pull(skb_t *skb, int len)
{
    assert(skb->refcnt > 0);
    return len < skb->len;
}

/*
 * Pops data from the SKB at the beginning of the data
 * section. Aborts if len is larger than the actual data
 * section length. If pulling from unknown sources, use
 * skb_may_pull() to check the length first. Returns a
 * pointer to the *new* beginning of the data section.
 */
void *
skb_pull(skb_t *skb, int len)
{
    assert(skb->refcnt > 0);
    assert(len < skb->len);
    skb->data += len;
    skb->len -= len;
    return skb->data;
}

/*
 * Appends data to the end of the data section. Aborts
 * if there is not enough space in the tail section to
 * cover the allocation. Returns a pointer to the *original*
 * end of the data section.
 */
void *
skb_put(skb_t *skb, int len)
{
    assert(skb->refcnt > 0);
    assert(skb->tail + len <= skb->end);
    void *orig_tail = skb->tail;
    skb->tail += len;
    skb->len += len;
    return orig_tail;
}

/*
 * Removes data from the end of the data section by setting
 * the length of the buffer. If the current length is greater
 * than the specified length, then this is a no-op.
 */
void
skb_trim(skb_t *skb, int len)
{
    assert(skb->refcnt > 0);
    if (len < skb->len) {
        skb->len = len;
        skb->tail = skb->data + len;
    }
}

/*
 * Reserves additional space in the head section. This
 * may only be called if the SKB is empty. It is not
 * necessary to reserve 2 bytes for IP header alignment,
 * since the buffer is pre-padded.
 */
void
skb_reserve(skb_t *skb, int len)
{
    assert(skb->refcnt > 0);
    assert(skb->len == 0);
    assert(skb->tail + len <= skb->end);
    skb->data += len;
    skb->tail += len;
}

/*
 * Marks the current start of the data section as the
 * Ethernet header.
 */
void *
skb_reset_mac_header(skb_t *skb)
{
    assert(skb->refcnt > 0);
    return skb->mac_header = skb->data;
}

/*
 * Marks the current start of the data section as the
 * IP header.
 */
void *
skb_reset_network_header(skb_t *skb)
{
    assert(skb->refcnt > 0);
    return skb->network_header = skb->data;
}

/*
 * Marks the current start of the data section as the
 * transport-layer header.
 */
void *
skb_reset_transport_header(skb_t *skb)
{
    assert(skb->refcnt > 0);
    return skb->transport_header = skb->data;
}

/*
 * Returns the MAC header, if set by skb_reset_mac_header().
 */
void *
skb_mac_header(skb_t *skb)
{
    assert(skb->refcnt > 0);
    return skb->mac_header;
}

/*
 * Returns the IP header, if set by skb_reset_network_header().
 */
void *
skb_network_header(skb_t *skb)
{
    assert(skb->refcnt > 0);
    return skb->network_header;
}

/*
 * Returns the transport header, if set by skb_reset_transport_header().
 */
void *
skb_transport_header(skb_t *skb)
{
    assert(skb->refcnt > 0);
    return skb->transport_header;
}
