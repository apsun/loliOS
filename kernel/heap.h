#ifndef _HEAP_H
#define _HEAP_H

#include "types.h"

#ifndef ASM

/* Heap state (can be either user or kernel) */
typedef struct {
    /* Virtual address at which this heap starts */
    uintptr_t start_vaddr;

    /* This heap may grow up to this address */
    uintptr_t end_vaddr;

    /* Whether the heap is kernel or userspace memory */
    bool user : 1;

    /* If true, this heap is currently mapped in memory */
    bool mapped : 1;

    /* Size of the heap in bytes, might not be a multiple of page size */
    int size;

    /* Number of valid entries in the page vector */
    int num_pages;

    /* Current capacity of page vector */
    int cap_pages;

    /* Vector of pages (physical addrs) that are allocated for this heap */
    uintptr_t *paddrs;
} heap_t;

/* Initializes a userspace heap */
void heap_init_user(heap_t *heap, uintptr_t start_vaddr, uintptr_t end_vaddr);

/* Initializes a kernel heap with preallocated paddrs array */
void heap_init_kernel(heap_t *heap, uintptr_t start_vaddr, uintptr_t end_vaddr, uintptr_t *paddrs);

/* Expands or shrinks the heap */
void *heap_sbrk(heap_t *heap, int delta);

/* Clones an existing heap */
int heap_clone(heap_t *dest, heap_t *src);

/* Removes memory mappings for the specified heap */
void heap_unmap(heap_t *heap);

/* Adds memory mappings for the specified heap */
void heap_map(heap_t *heap);

/* Frees all pages used by a heap */
void heap_clear(heap_t *heap);

#endif /* ASM */

#endif /* _HEAP_H */
