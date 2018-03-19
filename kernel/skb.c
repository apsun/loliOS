#include "skb.h"
#include "lib.h"
#include "debug.h"

/*
 * Since each SKB is about 1.5KB, we can't have too many
 * in the kernel page. If this limit becomes a problem,
 * we can allocate a dedicated 4MB page to hold the SKBs.
 */
static skb_t skb_cache[128];

/*
 * Allocates and initializes a new SKB. Returns NULL if
 * there are no free SKBs. The new SKB has reference count
 * initially set to 1.
 */
skb_t *
skb_alloc(void)
{
    int i;
    for (i = 0; i < array_len(skb_cache); ++i) {
        skb_t *skb = &skb_cache[i];
        if (skb->refcnt == 0) {
            skb->refcnt = 1;
            skb->len = 0;
            skb->head = &skb->buf[0];
            skb->data = &skb->buf[0];
            skb->tail = &skb->buf[0];
            skb->end = &skb->buf[sizeof(skb->buf)];
            skb->mac_header = NULL;
            skb->network_header = NULL;
            skb->transport_header = NULL;
            return skb;
        }
    }
    return NULL;
}

/*
 * Increments the reference count of a SKB. You should
 * have a corresponding call to skb_release() to decrement
 * the reference count.
 */
skb_t *
skb_retain(skb_t *skb)
{
    ASSERT(skb->refcnt > 0);
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
    ASSERT(skb->refcnt > 0);
    skb->refcnt--;
}

/*
 * Returns a pointer to the beginning of the data section.
 */
void *
skb_data(skb_t *skb)
{
    return skb->data;
}

/*
 * Pushes data into the SKB at the beginning of the data
 * section. Aborts if there is not enough space in the
 * head section to cover the allocation. Returns a pointer
 * to the *new* beginning of the data section.
 */
void *
skb_push(skb_t *skb, int len)
{
    ASSERT(skb->refcnt > 0);
    ASSERT(skb->data - len >= skb->head);
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
    ASSERT(skb->refcnt > 0);
    ASSERT(len < skb->len);
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
    ASSERT(skb->refcnt > 0);
    ASSERT(skb->tail + len <= skb->end);
    void *orig_tail = skb->tail;
    skb->tail += len;
    skb->len += len;
    return orig_tail;
}

/*
 * Reserves additional space in the head section. This
 * may only be called if the SKB is empty. It is not
 * necessary to reserve 2 bytes for the SKB in loliOS
 * to align the IP headers.
 */
void
skb_reserve(skb_t *skb, int len)
{
    ASSERT(skb->refcnt > 0);
    ASSERT(skb->len == 0);
    ASSERT(skb->tail + len <= skb->end);
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
    ASSERT(skb->refcnt > 0);
    return skb->mac_header = skb->data;
}

/*
 * Marks the current start of the data section as the
 * IP header.
 */
void *
skb_reset_network_header(skb_t *skb)
{
    ASSERT(skb->refcnt > 0);
    return skb->network_header = skb->data;
}

/*
 * Marks the current start of the data section as the
 * transport-layer header.
 */
void *
skb_reset_transport_header(skb_t *skb)
{
    ASSERT(skb->refcnt > 0);
    return skb->transport_header = skb->data;
}
