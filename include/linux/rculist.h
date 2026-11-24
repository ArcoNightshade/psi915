/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation from the documented behaviour of the Linux
 * RCU list API (Documentation/RCU/listRCU.rst). Updaters serialise among
 * themselves (a lock) and publish new entries with rcu_assign_pointer();
 * readers traverse under rcu_read_lock(). A removed entry keeps its next
 * pointer so concurrent readers can continue; it may only be freed after a
 * grace period (synchronize_rcu(), call_rcu(), kfree_rcu()). Removed
 * entries are not poisoned. The optional lockdep condition argument of the
 * iterators is accepted and ignored.
 */
#ifndef _LINUXKPI_LINUX_RCULIST_H_
#define	_LINUXKPI_LINUX_RCULIST_H_

#include <linuxkpi/platform.h>
#include <linux/compiler.h>
#include <linux/list.h>
#include <linux/rcupdate.h>

#define	list_next_rcu(list)	(*((struct list_head **)(&(list)->next)))
#define	list_tail_rcu(head)	(*((struct list_head **)(&(head)->prev)))

static inline void
INIT_LIST_HEAD_RCU(struct list_head *list)
{
	WRITE_ONCE(list->next, list);
	WRITE_ONCE(list->prev, list);
}

static inline void
__list_add_rcu(struct list_head *new, struct list_head *prev,
    struct list_head *next)
{
	new->next = next;
	new->prev = prev;
	rcu_assign_pointer(list_next_rcu(prev), new);
	next->prev = new;
}

static inline void
list_add_rcu(struct list_head *new, struct list_head *head)
{
	__list_add_rcu(new, head, head->next);
}

static inline void
list_add_tail_rcu(struct list_head *new, struct list_head *head)
{
	__list_add_rcu(new, head->prev, head);
}

/* Unlink entry; entry->next stays valid for readers already on it. */
static inline void
list_del_rcu(struct list_head *entry)
{
	__list_del_entry(entry);
}

static inline void
list_replace_rcu(struct list_head *old, struct list_head *new)
{
	new->next = old->next;
	new->prev = old->prev;
	rcu_assign_pointer(list_next_rcu(new->prev), new);
	new->next->prev = new;
}

#define	list_entry_rcu(ptr, type, member)				\
	container_of(READ_ONCE(ptr), type, member)

#define	list_first_or_null_rcu(ptr, type, member) ({			\
	struct list_head *__lkpi_lh = (ptr);				\
	struct list_head *__lkpi_ln = READ_ONCE(__lkpi_lh->next);	\
	__lkpi_ln != __lkpi_lh ?					\
	    list_entry_rcu(__lkpi_ln, type, member) : NULL;		\
})

#define	list_next_or_null_rcu(head, ptr, type, member) ({		\
	struct list_head *__lkpi_nh = (head);				\
	struct list_head *__lkpi_np = (ptr);				\
	struct list_head *__lkpi_nn = READ_ONCE(__lkpi_np->next);	\
	__lkpi_nn != __lkpi_nh ?					\
	    list_entry_rcu(__lkpi_nn, type, member) : NULL;		\
})

#define	list_for_each_entry_rcu(pos, head, member, ...)			\
	for ((pos) = list_entry_rcu((head)->next, typeof(*(pos)), member); \
	    &(pos)->member != (head);					\
	    (pos) = list_entry_rcu((pos)->member.next, typeof(*(pos)), member))

#define	list_for_each_entry_srcu(pos, head, member, cond)		\
	list_for_each_entry_rcu(pos, head, member)
#define	list_for_each_entry_lockless(pos, head, member)			\
	list_for_each_entry_rcu(pos, head, member)

#define	list_for_each_entry_continue_rcu(pos, head, member)		\
	for ((pos) = list_entry_rcu((pos)->member.next, typeof(*(pos)), member); \
	    &(pos)->member != (head);					\
	    (pos) = list_entry_rcu((pos)->member.next, typeof(*(pos)), member))

#define	list_for_each_entry_from_rcu(pos, head, member)			\
	for (; &(pos)->member != (head);				\
	    (pos) = list_entry_rcu((pos)->member.next, typeof(*(pos)), member))

/* hlist */
#define	hlist_first_rcu(head)	(*((struct hlist_node **)(&(head)->first)))
#define	hlist_next_rcu(node)	(*((struct hlist_node **)(&(node)->next)))
#define	hlist_pprev_rcu(node)	(*((struct hlist_node **)((node)->pprev)))

static inline void
hlist_add_head_rcu(struct hlist_node *n, struct hlist_head *h)
{
	struct hlist_node *first = h->first;

	n->next = first;
	WRITE_ONCE(n->pprev, &h->first);
	rcu_assign_pointer(hlist_first_rcu(h), n);
	if (first != NULL)
		WRITE_ONCE(first->pprev, &n->next);
}

static inline void
hlist_add_tail_rcu(struct hlist_node *n, struct hlist_head *h)
{
	struct hlist_node *i, *last = NULL;

	for (i = h->first; i != NULL; i = i->next)
		last = i;
	if (last == NULL) {
		hlist_add_head_rcu(n, h);
		return;
	}
	n->next = NULL;
	WRITE_ONCE(n->pprev, &last->next);
	rcu_assign_pointer(hlist_next_rcu(last), n);
}

static inline void
hlist_add_before_rcu(struct hlist_node *n, struct hlist_node *next)
{
	WRITE_ONCE(n->pprev, next->pprev);
	n->next = next;
	rcu_assign_pointer(hlist_pprev_rcu(n), n);
	WRITE_ONCE(next->pprev, &n->next);
}

static inline void
hlist_add_behind_rcu(struct hlist_node *n, struct hlist_node *prev)
{
	n->next = prev->next;
	WRITE_ONCE(n->pprev, &prev->next);
	rcu_assign_pointer(hlist_next_rcu(prev), n);
	if (n->next != NULL)
		WRITE_ONCE(n->next->pprev, &n->next);
}

static inline void
hlist_del_rcu(struct hlist_node *n)
{
	__hlist_del(n);
}

static inline void
hlist_del_init_rcu(struct hlist_node *n)
{
	if (!hlist_unhashed(n)) {
		__hlist_del(n);
		WRITE_ONCE(n->pprev, NULL);
	}
}

static inline void
hlist_replace_rcu(struct hlist_node *old, struct hlist_node *new)
{
	struct hlist_node *next = old->next;

	new->next = next;
	WRITE_ONCE(new->pprev, old->pprev);
	rcu_assign_pointer(*(struct hlist_node **)new->pprev, new);
	if (next != NULL)
		WRITE_ONCE(new->next->pprev, &new->next);
}

#define	hlist_for_each_entry_rcu(pos, head, member, ...)		\
	for ((pos) = hlist_entry_safe(rcu_dereference_raw(hlist_first_rcu(head)), \
	    typeof(*(pos)), member);					\
	    (pos) != NULL;						\
	    (pos) = hlist_entry_safe(rcu_dereference_raw(		\
	    hlist_next_rcu(&(pos)->member)), typeof(*(pos)), member))

#define	hlist_for_each_entry_srcu(pos, head, member, cond)		\
	hlist_for_each_entry_rcu(pos, head, member)

#define	hlist_for_each_entry_continue_rcu(pos, member)			\
	for ((pos) = hlist_entry_safe(rcu_dereference_raw(		\
	    hlist_next_rcu(&(pos)->member)), typeof(*(pos)), member);	\
	    (pos) != NULL;						\
	    (pos) = hlist_entry_safe(rcu_dereference_raw(		\
	    hlist_next_rcu(&(pos)->member)), typeof(*(pos)), member))

#define	hlist_for_each_entry_from_rcu(pos, member)			\
	for (; (pos) != NULL;						\
	    (pos) = hlist_entry_safe(rcu_dereference_raw(		\
	    hlist_next_rcu(&(pos)->member)), typeof(*(pos)), member))

#endif /* _LINUXKPI_LINUX_RCULIST_H_ */
