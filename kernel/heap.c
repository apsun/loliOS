#include "heap.h"
#include "debug.h"
#include "types.h"
#include "paging.h"
#include "string.h"

/*
 * Initializes a new heap.
 */
void
heap_init(heap_t *heap, uintptr_t start_vaddr, uintptr_t end_vaddr, bool user)
{
    int npages = (heap->end_vaddr - heap->start_vaddr + PAGE_SIZE - 1) / PAGE_SIZE;
    assert(npages <= MAX_HEAP_PAGES);

    heap->start_vaddr = start_vaddr;
    heap->end_vaddr = end_vaddr;
    heap->user = user;
    heap->mapped = false;
    heap->size = 0;
    heap->num_pages = 0;
}

/*
 * Shrinks the specified heap to the specified number of pages.
 */
static void
heap_shrink(heap_t *heap, int new_pages)
{
    assert(new_pages >= 0 && new_pages <= MAX_HEAP_PAGES);

    while (heap->num_pages > new_pages) {
        int i = --heap->num_pages;
        uintptr_t vaddr = heap->start_vaddr + i * PAGE_SIZE;
        uintptr_t paddr = heap->page_paddrs[i];

        if (heap->mapped) {
            paging_page_unmap(vaddr);
        }

        paging_page_free(paddr);
    }
}

/*
 * Grows the specified heap to the specified number of pages. If
 * there are not enough free pages to satisfy the allocation, the
 * heap will not be modified and -1 will be returned. Returns 0
 * on success.
 */
static int
heap_grow(heap_t *heap, int new_pages)
{
    assert(new_pages >= 0 && new_pages <= MAX_HEAP_PAGES);

    int orig_num_pages = heap->num_pages;
    while (heap->num_pages < new_pages) {
        uintptr_t vaddr = heap->start_vaddr + heap->num_pages * PAGE_SIZE;
        uintptr_t paddr = paging_page_alloc();

        if (paddr == 0) {
            debugf("Physical memory exhausted\n");
            heap_shrink(heap, orig_num_pages);
            return -1;
        }

        if (heap->mapped) {
            paging_page_map(vaddr, paddr, heap->user);
        }

        heap->page_paddrs[heap->num_pages++] = paddr;
    }

    return 0;
}

/*
 * Grows or shrinks a heap, depending on the value
 * of delta. Returns NULL on error (e.g. shrinking by
 * more than available, or not enough physical memory).
 * On success, returns the previous brk's virtual address.
 * This function is guaranteed to not fail if delta == 0.
 *
 * The heap MUST currently be mapped in memory!
 */
void *
heap_sbrk(heap_t *heap, int delta)
{
    assert(heap->mapped);

    int orig_size = heap->size;
    void *orig_brk = (void *)(heap->start_vaddr + orig_size);

    if (delta == 0) {
        return orig_brk;
    }

    /* Upper bound limit (if delta is huge, rhs is negative -> true) */
    int max_heap_size = heap->end_vaddr - heap->start_vaddr;
    if (delta > 0 && orig_size > max_heap_size - delta) {
        debugf("Trying to expand heap beyond size limit\n");
        return NULL;
    }

    /* Lower bound limit */
    if (delta < 0 && orig_size + delta < 0) {
        debugf("Trying to deallocate more than was allocated\n");
        return NULL;
    }

    int new_size = orig_size + delta;
    int new_num_pages = (new_size + PAGE_SIZE - 1) / PAGE_SIZE;

    /* Grow or shrink heap as necessary */
    int orig_num_pages = heap->num_pages;
    void *orig_page_brk = (void *)(heap->start_vaddr + orig_num_pages * PAGE_SIZE);
    if (new_num_pages > orig_num_pages) {
        if (heap_grow(heap, new_num_pages) < 0) {
            return NULL;
        }

        /* Clear newly allocated pages (heap must be mapped in memory) */
        memset(orig_page_brk, 0, (new_num_pages - orig_num_pages) * PAGE_SIZE);
    } else if (new_num_pages < orig_num_pages) {
        heap_shrink(heap, new_num_pages);
    }

    heap->size = new_size;
    return orig_brk;
}

/*
 * Clones an existing process heap. Note that this currently does
 * not perform copy-on-write optimization.
 *
 * The src heap MUST currently be mapped in memory!
 */
int
heap_clone(heap_t *dest, heap_t *src)
{
    assert(src->mapped);

    /* Copy properties */
    dest->start_vaddr = src->start_vaddr;
    dest->end_vaddr = src->end_vaddr;
    dest->user = src->user;
    dest->mapped = false;

    /* Allocate same number of pages as src */
    dest->num_pages = 0;
    if (heap_grow(dest, src->num_pages) < 0) {
        return -1;
    }

    /*
     * Copy page contents. Temporary page is necessary since
     * we need to be able to view both physical addresses
     * simultaneously, but both have the same virtual address.
     */
    int i;
    for (i = 0; i < dest->num_pages; ++i) {
        paging_page_map(TEMP_PAGE_START, dest->page_paddrs[i], false);
        uintptr_t src_vaddr = src->start_vaddr + i * PAGE_SIZE;
        memcpy((void *)TEMP_PAGE_START, (const void *)src_vaddr, PAGE_SIZE);
    }

    paging_page_unmap(TEMP_PAGE_START);
    dest->size = src->size;
    return 0;
}

/*
 * Removes memory mappings for the specified heap.
 */
void
heap_unmap(heap_t *heap)
{
    assert(heap->mapped);
    heap->mapped = false;

    int i;
    for (i = 0; i < heap->num_pages; ++i) {
        uintptr_t vaddr = heap->start_vaddr + i * PAGE_SIZE;
        paging_page_unmap(vaddr);
    }
}

/*
 * Adds memory mappings for the specified heap.
 */
void
heap_map(heap_t *heap)
{
    assert(!heap->mapped);
    heap->mapped = true;

    int i;
    for (i = 0; i < heap->num_pages; ++i) {
        uintptr_t vaddr = heap->start_vaddr + i * PAGE_SIZE;
        paging_page_map(vaddr, heap->page_paddrs[i], heap->user);
    }
}

/*
 * Deallocates a heap, freeing all pages used by it.
 * This restores the heap to its initial (empty) state.
 */
void
heap_clear(heap_t *heap)
{
    heap->size = 0;
    heap_shrink(heap, 0);
}
