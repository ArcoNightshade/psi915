/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Lock-less singly linked lists (Linux llist semantics): any number of
 * concurrent llist_add() callers, and llist_del_all()/llist_del_first() by
 * consumers (llist_del_first only from one consumer at a time). Built on
 * compare-and-swap; entries come back newest first.
 */
#ifndef _LINUXKPI_LINUX_LLIST_H_
#define	_LINUXKPI_LINUX_LLIST_H_

#include <linux/types.h>
#include <linux/compiler.h>
#include <linux/container_of.h>
#include <linux/atomic.h>

struct llist_node {
	struct llist_node	*next;
};

struct llist_head {
	struct llist_node	*first;
};

#define	LLIST_HEAD_INIT(name)	{ NULL }
#define	LLIST_HEAD(name)	struct llist_head name = LLIST_HEAD_INIT(name)

static inline void
init_llist_head(struct llist_head *h)
{
	h->first = NULL;
}

static inline void
init_llist_node(struct llist_node *n)
{
	n->next = n;
}

static inline bool
llist_on_list(const struct llist_node *n)
{
	return (n->next != n);
}

static inline bool
llist_empty(const struct llist_head *h)
{
	return (READ_ONCE(h->first) == NULL);
}

static inline struct llist_node *
llist_next(struct llist_node *n)
{
	return (n->next);
}

/* Add first..last (already linked); true if the list was empty. */
static inline bool
llist_add_batch(struct llist_node *first, struct llist_node *last,
    struct llist_head *h)
{
	struct llist_node *old = READ_ONCE(h->first);

	do {
		last->next = old;
	} while (!__atomic_compare_exchange_n(&h->first, &old, first, false,
	    __ATOMIC_RELEASE, __ATOMIC_RELAXED));
	return (old == NULL);
}

static inline bool
llist_add(struct llist_node *n, struct llist_head *h)
{
	return (llist_add_batch(n, n, h));
}

static inline struct llist_node *
llist_del_all(struct llist_head *h)
{
	return (__atomic_exchange_n(&h->first, NULL, __ATOMIC_ACQUIRE));
}

static inline struct llist_node *
llist_del_first(struct llist_head *h)
{
	struct llist_node *e = READ_ONCE(h->first), *next;

	do {
		if (e == NULL)
			return (NULL);
		next = READ_ONCE(e->next);
	} while (!__atomic_compare_exchange_n(&h->first, &e, next, false,
	    __ATOMIC_ACQUIRE, __ATOMIC_ACQUIRE));
	return (e);
}

static inline struct llist_node *
llist_reverse_order(struct llist_node *head)
{
	struct llist_node *r = NULL, *t;

	while (head != NULL) {
		t = head->next;
		head->next = r;
		r = head;
		head = t;
	}
	return (r);
}

#define	llist_entry(ptr, type, member)	container_of(ptr, type, member)
#define	llist_for_each(pos, node)					\
	for ((pos) = (node); (pos) != NULL; (pos) = (pos)->next)
#define	llist_for_each_safe(pos, n, node)				\
	for ((pos) = (node); (pos) != NULL && ((n) = (pos)->next, 1);	\
	    (pos) = (n))
#define	llist_for_each_entry(pos, node, member)				\
	for ((pos) = (node) != NULL ?					\
	    llist_entry((node), typeof(*(pos)), member) : NULL;		\
	    (pos) != NULL;						\
	    (pos) = (pos)->member.next != NULL ?			\
	    llist_entry((pos)->member.next, typeof(*(pos)), member) : NULL)
#define	llist_for_each_entry_safe(pos, n, node, member)			\
	for ((pos) = (node) != NULL ?					\
	    llist_entry((node), typeof(*(pos)), member) : NULL;		\
	    (pos) != NULL && ((n) = (pos)->member.next != NULL ?	\
	    llist_entry((pos)->member.next, typeof(*(pos)), member) :	\
	    NULL, 1);							\
	    (pos) = (n))

#endif /* _LINUXKPI_LINUX_LLIST_H_ */
