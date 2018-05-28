#ifndef _LIST_H
#define _LIST_H

#include "lib.h"

#ifndef ASM

/*
 * Intrusive linked list node structure, just like the one
 * in Linux.
 */
typedef struct list {
    struct list *prev;
    struct list *next;
} list_t;

/*
 * Declares a new empty linked list.
 */
#define list_declare(name) \
    list_t name = { .prev = &(name), .next = &(name) }

/*
 * Returns a pointer to the structure containing
 * this linked list node.
 */
#define list_entry(ptr, type, member) \
    container_of(ptr, type_member)

/*
 * Forward iteration helper for linked list, replaces "for (...)".
 */
#define list_for_each(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

/*
 * Reverse iteration helper for linked list, replaces "for (...)".
 */
#define list_for_each_prev(pos, head) \
    for (pos = (head)->prev; pos != (head); pos = pos->prev)

/*
 * Adds a node to the head of the specified linked list.
 */
static inline void
list_add(list_t *node, list_t *head)
{
    node->prev = head;
    node->next = head->next;
    head->next->prev = node;
    head->next = node;
}

/*
 * Adds a node to the tail of the specified linked list.
 */
static inline void
list_add_tail(list_t *node, list_t *head)
{
    node->next = head;
    node->prev = head->prev;
    head->prev->next = node;
    head->prev = node;
}

/*
 * Removes the specified node from its linked list.
 */
static inline void
list_del(list_t *node)
{
    node->next->prev = node->prev;
    node->prev->next = node->next;
    node->next = NULL;
    node->prev = NULL;
}

#endif /* ASM */

#endif /* _LIST_H */
