#include "skb.h"
#include "lib.h"
#include "debug.h"

#define MAX_SKBS 16

/* Array of skb structures */
skb_t skb_cache[MAX_SKBS];

/*
 * Allocates and initializes a new SKB. Returns NULL if
 * there are no free SKBs.
 */
skb_t *
skb_alloc(void)
{
    int i;
    for (i = 0; i < MAX_SKBS; ++i) {
        skb_t *skb = &skb_cache[i];
        if (!skb->used) {
            skb->used = 1;
            skb->head = &skb->buf[0];
            skb->data = &skb->buf[0];
            skb->tail = &skb->buf[0];
            skb->end = &skb->buf[sizeof(skb->buf)];
            skb->len = 0;
            skb->mac_header = NULL;
            skb->network_header = NULL;
            skb->transport_header = NULL;
            return skb;
        }
    }
    return NULL;
}

void *
skb_push(skb_t *skb, int len)
{
    skb->data -= len;
    skb->len += len;
    return skb->data;
}

void *
skb_pull(skb_t *skb, int len)
{
    skb->data += len;
    skb->len -= len;
    return skb->data;
}

void *
skb_put(skb_t *skb, int len)
{
    void *orig_tail = skb->tail;
    skb->tail += len;
    skb->len += len;
    return orig_tail;
}

void
skb_reserve(skb_t *skb, int len)
{
    skb->data += len;
    skb->tail += len;
}

void
skb_free(skb_t *skb)
{
    skb->used = 0;
}
