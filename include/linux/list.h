/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013-2016 Mellanox Technologies, Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Derived from FreeBSD sys/compat/linuxkpi/common/include/linux/list.h
 * (freebsd-src e5477acb8a8b), BSD-2-Clause, notice above preserved.
 *
 * Modifications for psi915 (illumos LinuxKPI):
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * SPDX-License-Identifier: BSD-2-Clause
 *  - FreeBSD kernel includes replaced by <linuxkpi/platform.h>; struct
 *    list_head and hlist_* are declared in <linux/types.h>
 *  - added list_swap, list_rotate_left, list_is_head, list_cut_before,
 *    list_entry_is_head and the *_safe_continue iterator
 *  - list_rotate_to_front follows the Linux semantics (entry becomes first)
 *  - splice helpers check for an empty source list themselves
 *  - list_sort() is in <linux/list_sort.h>, RCU variants in <linux/rculist.h>
 */
#ifndef _LINUXKPI_LINUX_LIST_H_
#define	_LINUXKPI_LINUX_LIST_H_

#include <linuxkpi/platform.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/container_of.h>

/*
 * Note: list_del() does not poison the removed entry (Linux writes
 * LIST_POISON1/2). Code must not rely on a deleted entry's pointers.
 */

#define	LIST_HEAD_INIT(name)	{ &(name), &(name) }

/* <sys/queue.h> defines a different LIST_HEAD; Linux code wants this one. */
#undef	LIST_HEAD
#define	LIST_HEAD(name)		struct list_head name = LIST_HEAD_INIT(name)

static inline void
INIT_LIST_HEAD(struct list_head *list)
{
	WRITE_ONCE(list->next, list);
	list->prev = list;
}

static inline int
list_empty(const struct list_head *head)
{
	return (READ_ONCE(head->next) == head);
}

static inline int
list_empty_careful(const struct list_head *head)
{
	struct list_head *next = head->next;

	return ((next == head) && (next == head->prev));
}

static inline void
__list_add(struct list_head *new, struct list_head *prev,
    struct list_head *next)
{
	next->prev = new;
	new->next = next;
	new->prev = prev;
	WRITE_ONCE(prev->next, new);
}

static inline void
list_add(struct list_head *new, struct list_head *head)
{
	__list_add(new, head, head->next);
}

static inline void
list_add_tail(struct list_head *new, struct list_head *head)
{
	__list_add(new, head->prev, head);
}

static inline void
__list_del(struct list_head *prev, struct list_head *next)
{
	next->prev = prev;
	WRITE_ONCE(prev->next, next);
}

static inline void
__list_del_entry(struct list_head *entry)
{
	__list_del(entry->prev, entry->next);
}

static inline void
list_del(struct list_head *entry)
{
	__list_del(entry->prev, entry->next);
}

static inline void
list_del_init(struct list_head *entry)
{
	__list_del_entry(entry);
	INIT_LIST_HEAD(entry);
}

static inline void
list_replace(struct list_head *old, struct list_head *new)
{
	new->next = old->next;
	new->next->prev = new;
	new->prev = old->prev;
	new->prev->next = new;
}

static inline void
list_replace_init(struct list_head *old, struct list_head *new)
{
	list_replace(old, new);
	INIT_LIST_HEAD(old);
}

static inline void
list_swap(struct list_head *entry1, struct list_head *entry2)
{
	struct list_head *pos = entry2->prev;

	list_del(entry2);
	list_replace(entry1, entry2);
	if (pos == entry1)
		pos = entry2;
	list_add(entry1, pos);
}

static inline void
list_move(struct list_head *entry, struct list_head *head)
{
	__list_del_entry(entry);
	list_add(entry, head);
}

static inline void
list_move_tail(struct list_head *entry, struct list_head *head)
{
	__list_del_entry(entry);
	list_add_tail(entry, head);
}

static inline void
list_bulk_move_tail(struct list_head *head, struct list_head *first,
    struct list_head *last)
{
	first->prev->next = last->next;
	last->next->prev = first->prev;
	head->prev->next = first;
	first->prev = head->prev;
	last->next = head;
	head->prev = last;
}

static inline void
list_rotate_left(struct list_head *head)
{
	if (!list_empty(head))
		list_move_tail(head->next, head);
}

static inline void
list_rotate_to_front(struct list_head *entry, struct list_head *head)
{
	list_move_tail(head, entry);
}

static inline int
list_is_first(const struct list_head *entry, const struct list_head *head)
{
	return (entry->prev == head);
}

static inline int
list_is_last(const struct list_head *entry, const struct list_head *head)
{
	return (entry->next == head);
}

static inline int
list_is_head(const struct list_head *entry, const struct list_head *head)
{
	return (entry == head);
}

static inline int
list_is_singular(const struct list_head *head)
{
	return (!list_empty(head) && (head->next == head->prev));
}

static inline void
__list_cut_position(struct list_head *list, struct list_head *head,
    struct list_head *entry)
{
	struct list_head *new_first = entry->next;

	list->next = head->next;
	list->next->prev = list;
	list->prev = entry;
	entry->next = list;
	head->next = new_first;
	new_first->prev = head;
}

/*
 * Move the entries of head up to and including entry onto list (which is
 * overwritten). entry must be on head, or be head itself.
 */
static inline void
list_cut_position(struct list_head *list, struct list_head *head,
    struct list_head *entry)
{
	if (list_empty(head))
		return;
	if (list_is_singular(head) && head->next != entry && head != entry)
		return;
	if (entry == head)
		INIT_LIST_HEAD(list);
	else
		__list_cut_position(list, head, entry);
}

/* Move the entries of head before entry onto list (overwritten). */
static inline void
list_cut_before(struct list_head *list, struct list_head *head,
    struct list_head *entry)
{
	if (head->next == entry) {
		INIT_LIST_HEAD(list);
		return;
	}
	list->next = head->next;
	list->next->prev = list;
	list->prev = entry->prev;
	list->prev->next = list;
	head->next = entry;
	entry->prev = head;
}

static inline void
__list_splice(const struct list_head *list, struct list_head *prev,
    struct list_head *next)
{
	struct list_head *first = list->next;
	struct list_head *last = list->prev;

	first->prev = prev;
	prev->next = first;
	last->next = next;
	next->prev = last;
}

static inline void
list_splice(const struct list_head *list, struct list_head *head)
{
	if (!list_empty(list))
		__list_splice(list, head, head->next);
}

static inline void
list_splice_tail(struct list_head *list, struct list_head *head)
{
	if (!list_empty(list))
		__list_splice(list, head->prev, head);
}

static inline void
list_splice_init(struct list_head *list, struct list_head *head)
{
	if (!list_empty(list)) {
		__list_splice(list, head, head->next);
		INIT_LIST_HEAD(list);
	}
}

static inline void
list_splice_tail_init(struct list_head *list, struct list_head *head)
{
	if (!list_empty(list)) {
		__list_splice(list, head->prev, head);
		INIT_LIST_HEAD(list);
	}
}

static inline size_t
list_count_nodes(const struct list_head *head)
{
	const struct list_head *p;
	size_t count = 0;

	for (p = head->next; p != head; p = p->next)
		count++;
	return (count);
}

#define	list_entry(ptr, type, member)	container_of(ptr, type, member)

#define	list_first_entry(ptr, type, member)				\
	list_entry((ptr)->next, type, member)

#define	list_last_entry(ptr, type, member)				\
	list_entry((ptr)->prev, type, member)

#define	list_first_entry_or_null(ptr, type, member) ({			\
	struct list_head *__lkpi_head = (ptr);				\
	struct list_head *__lkpi_pos = READ_ONCE(__lkpi_head->next);	\
	__lkpi_pos != __lkpi_head ?					\
	    list_entry(__lkpi_pos, type, member) : NULL;		\
})

#define	list_next_entry(pos, member)					\
	list_entry((pos)->member.next, typeof(*(pos)), member)

#define	list_prev_entry(pos, member)					\
	list_entry((pos)->member.prev, typeof(*(pos)), member)

#define	list_entry_is_head(pos, head, member)				\
	(&(pos)->member == (head))

#define	list_safe_reset_next(pos, n, member)				\
	(n) = list_next_entry(pos, member)

#define	list_for_each(p, head)						\
	for ((p) = (head)->next; (p) != (head); (p) = (p)->next)

#define	list_for_each_prev(p, head)					\
	for ((p) = (head)->prev; (p) != (head); (p) = (p)->prev)

#define	list_for_each_safe(p, n, head)					\
	for ((p) = (head)->next, (n) = (p)->next; (p) != (head);	\
	    (p) = (n), (n) = (p)->next)

#define	list_for_each_prev_safe(p, n, head)				\
	for ((p) = (head)->prev, (n) = (p)->prev; (p) != (head);	\
	    (p) = (n), (n) = (p)->prev)

#define	list_for_each_entry(pos, head, member)				\
	for ((pos) = list_first_entry(head, typeof(*(pos)), member);	\
	    !list_entry_is_head(pos, head, member);			\
	    (pos) = list_next_entry(pos, member))

#define	list_for_each_entry_reverse(pos, head, member)			\
	for ((pos) = list_last_entry(head, typeof(*(pos)), member);	\
	    !list_entry_is_head(pos, head, member);			\
	    (pos) = list_prev_entry(pos, member))

#define	list_for_each_entry_continue(pos, head, member)			\
	for ((pos) = list_next_entry(pos, member);			\
	    !list_entry_is_head(pos, head, member);			\
	    (pos) = list_next_entry(pos, member))

#define	list_for_each_entry_continue_reverse(pos, head, member)		\
	for ((pos) = list_prev_entry(pos, member);			\
	    !list_entry_is_head(pos, head, member);			\
	    (pos) = list_prev_entry(pos, member))

#define	list_for_each_entry_from(pos, head, member)			\
	for (; !list_entry_is_head(pos, head, member);			\
	    (pos) = list_next_entry(pos, member))

#define	list_for_each_entry_from_reverse(pos, head, member)		\
	for (; !list_entry_is_head(pos, head, member);			\
	    (pos) = list_prev_entry(pos, member))

#define	list_for_each_entry_safe(pos, n, head, member)			\
	for ((pos) = list_first_entry(head, typeof(*(pos)), member),	\
	    (n) = list_next_entry(pos, member);				\
	    !list_entry_is_head(pos, head, member);			\
	    (pos) = (n), (n) = list_next_entry(n, member))

#define	list_for_each_entry_safe_continue(pos, n, head, member)		\
	for ((pos) = list_next_entry(pos, member),			\
	    (n) = list_next_entry(pos, member);				\
	    !list_entry_is_head(pos, head, member);			\
	    (pos) = (n), (n) = list_next_entry(n, member))

#define	list_for_each_entry_safe_from(pos, n, head, member)		\
	for ((n) = list_next_entry(pos, member);			\
	    !list_entry_is_head(pos, head, member);			\
	    (pos) = (n), (n) = list_next_entry(n, member))

#define	list_for_each_entry_safe_reverse(pos, n, head, member)		\
	for ((pos) = list_last_entry(head, typeof(*(pos)), member),	\
	    (n) = list_prev_entry(pos, member);				\
	    !list_entry_is_head(pos, head, member);			\
	    (pos) = (n), (n) = list_prev_entry(n, member))

/*
 * hlist: singly-headed doubly linked list (hash buckets).
 */
#define	HLIST_HEAD_INIT		{ .first = NULL }
#define	HLIST_HEAD(name)	struct hlist_head name = HLIST_HEAD_INIT
#define	INIT_HLIST_HEAD(head)	((head)->first = NULL)

static inline void
INIT_HLIST_NODE(struct hlist_node *n)
{
	n->next = NULL;
	n->pprev = NULL;
}

static inline int
hlist_unhashed(const struct hlist_node *n)
{
	return (n->pprev == NULL);
}

static inline int
hlist_empty(const struct hlist_head *h)
{
	return (READ_ONCE(h->first) == NULL);
}

static inline void
__hlist_del(struct hlist_node *n)
{
	struct hlist_node *next = n->next;
	struct hlist_node **pprev = n->pprev;

	WRITE_ONCE(*pprev, next);
	if (next != NULL)
		next->pprev = pprev;
}

static inline void
hlist_del(struct hlist_node *n)
{
	__hlist_del(n);
}

static inline void
hlist_del_init(struct hlist_node *n)
{
	if (!hlist_unhashed(n)) {
		__hlist_del(n);
		INIT_HLIST_NODE(n);
	}
}

static inline void
hlist_add_head(struct hlist_node *n, struct hlist_head *h)
{
	struct hlist_node *first = h->first;

	n->next = first;
	if (first != NULL)
		first->pprev = &n->next;
	WRITE_ONCE(h->first, n);
	n->pprev = &h->first;
}

static inline void
hlist_add_before(struct hlist_node *n, struct hlist_node *next)
{
	n->pprev = next->pprev;
	n->next = next;
	next->pprev = &n->next;
	WRITE_ONCE(*(n->pprev), n);
}

static inline void
hlist_add_behind(struct hlist_node *n, struct hlist_node *prev)
{
	n->next = prev->next;
	WRITE_ONCE(prev->next, n);
	n->pprev = &prev->next;
	if (n->next != NULL)
		n->next->pprev = &n->next;
}

static inline void
hlist_move_list(struct hlist_head *old, struct hlist_head *new)
{
	new->first = old->first;
	if (new->first != NULL)
		new->first->pprev = &new->first;
	old->first = NULL;
}

#define	hlist_entry(ptr, type, member)	container_of(ptr, type, member)

#define	hlist_entry_safe(ptr, type, member) ({				\
	typeof(ptr) __lkpi_hptr = (ptr);				\
	__lkpi_hptr != NULL ?						\
	    hlist_entry(__lkpi_hptr, type, member) : NULL;		\
})

#define	hlist_for_each(p, head)						\
	for ((p) = (head)->first; (p) != NULL; (p) = (p)->next)

#define	hlist_for_each_safe(p, n, head)					\
	for ((p) = (head)->first; (p) != NULL && ({ (n) = (p)->next; 1; }); \
	    (p) = (n))

#define	hlist_for_each_entry(pos, head, member)				\
	for ((pos) = hlist_entry_safe((head)->first, typeof(*(pos)), member); \
	    (pos) != NULL;						\
	    (pos) = hlist_entry_safe((pos)->member.next, typeof(*(pos)), member))

#define	hlist_for_each_entry_continue(pos, member)			\
	for ((pos) = hlist_entry_safe((pos)->member.next, typeof(*(pos)), member); \
	    (pos) != NULL;						\
	    (pos) = hlist_entry_safe((pos)->member.next, typeof(*(pos)), member))

#define	hlist_for_each_entry_from(pos, member)				\
	for (; (pos) != NULL;						\
	    (pos) = hlist_entry_safe((pos)->member.next, typeof(*(pos)), member))

#define	hlist_for_each_entry_safe(pos, n, head, member)			\
	for ((pos) = hlist_entry_safe((head)->first, typeof(*(pos)), member); \
	    (pos) != NULL && ({ (n) = (pos)->member.next; 1; });	\
	    (pos) = hlist_entry_safe(n, typeof(*(pos)), member))

#endif /* _LINUXKPI_LINUX_LIST_H_ */
