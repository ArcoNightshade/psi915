/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation of the Linux fixed-size hash table helpers
 * (an array of hlist heads indexed by hash_min()) from their documented
 * behaviour; function set cross-checked against FreeBSD's LinuxKPI
 * hashtable.h (BSD-2-Clause).
 */
#ifndef _LINUXKPI_LINUX_HASHTABLE_H_
#define	_LINUXKPI_LINUX_HASHTABLE_H_

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/hash.h>
#include <linux/log2.h>
#include <linux/rculist.h>

#define	DEFINE_HASHTABLE(name, bits)					\
	struct hlist_head name[1 << (bits)] =				\
	    { [0 ... ((1 << (bits)) - 1)] = HLIST_HEAD_INIT }
#define	DEFINE_READ_MOSTLY_HASHTABLE(name, bits) DEFINE_HASHTABLE(name, bits)
#define	DECLARE_HASHTABLE(name, bits)	struct hlist_head name[1 << (bits)]

#define	HASH_SIZE(name)		(ARRAY_SIZE(name))
#define	HASH_BITS(name)		ilog2(HASH_SIZE(name))

#define	hash_min(val, bits)						\
	(sizeof (val) <= 4 ? hash_32((u32)(val), bits) :		\
	    hash_long((unsigned long)(val), bits))

static inline void
__hash_init(struct hlist_head *ht, unsigned int sz)
{
	unsigned int i;

	for (i = 0; i < sz; i++)
		INIT_HLIST_HEAD(&ht[i]);
}

#define	hash_init(ht)		__hash_init(ht, HASH_SIZE(ht))
#define	hash_add(ht, node, key)						\
	hlist_add_head(node, &(ht)[hash_min(key, HASH_BITS(ht))])
#define	hash_add_rcu(ht, node, key)					\
	hlist_add_head_rcu(node, &(ht)[hash_min(key, HASH_BITS(ht))])
#define	hash_hashed(node)	(!hlist_unhashed(node))

static inline bool
__hash_empty(struct hlist_head *ht, unsigned int sz)
{
	unsigned int i;

	for (i = 0; i < sz; i++)
		if (!hlist_empty(&ht[i]))
			return (false);
	return (true);
}

#define	hash_empty(ht)		__hash_empty(ht, HASH_SIZE(ht))

static inline void
hash_del(struct hlist_node *node)
{
	hlist_del_init(node);
}

static inline void
hash_del_rcu(struct hlist_node *node)
{
	hlist_del_init_rcu(node);
}

#define	hash_for_each(name, bkt, obj, member)				\
	for ((bkt) = 0; (bkt) < HASH_SIZE(name); (bkt)++)		\
		hlist_for_each_entry(obj, &(name)[bkt], member)
#define	hash_for_each_rcu(name, bkt, obj, member)			\
	for ((bkt) = 0; (bkt) < HASH_SIZE(name); (bkt)++)		\
		hlist_for_each_entry_rcu(obj, &(name)[bkt], member)
#define	hash_for_each_safe(name, bkt, tmp, obj, member)			\
	for ((bkt) = 0; (bkt) < HASH_SIZE(name); (bkt)++)		\
		hlist_for_each_entry_safe(obj, tmp, &(name)[bkt], member)
#define	hash_for_each_possible(name, obj, member, key)			\
	hlist_for_each_entry(obj,					\
	    &(name)[hash_min(key, HASH_BITS(name))], member)
#define	hash_for_each_possible_rcu(name, obj, member, key, ...)		\
	hlist_for_each_entry_rcu(obj,					\
	    &(name)[hash_min(key, HASH_BITS(name))], member)
#define	hash_for_each_possible_safe(name, obj, tmp, member, key)	\
	hlist_for_each_entry_safe(obj, tmp,				\
	    &(name)[hash_min(key, HASH_BITS(name))], member)

#endif /* _LINUXKPI_LINUX_HASHTABLE_H_ */
