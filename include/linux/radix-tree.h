/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Radix trees over the KPI XArray (as in Linux, where the radix tree
 * API is a wrapper of the XArray). Lookups take the XArray lock instead
 * of being lockless.
 */
#ifndef _LINUXKPI_INCLUDE_LINUX_RADIX_TREE_H_
#define	_LINUXKPI_INCLUDE_LINUX_RADIX_TREE_H_

#include <linux/types.h>
#include <linux/xarray.h>
#include <linux/gfp.h>

#define	radix_tree_root		xarray
#define	RADIX_TREE_INIT(name, mask)	XARRAY_INIT(name, mask)
#define	RADIX_TREE(name, mask)		struct xarray name = RADIX_TREE_INIT(name, mask)
#define	INIT_RADIX_TREE(root, mask)	xa_init_flags(root, mask)

static inline int
radix_tree_insert(struct xarray *xa, unsigned long index, void *item)
{
	return (xa_insert(xa, index, item, GFP_KERNEL | __GFP_NOWARN));
}

#define	radix_tree_lookup(xa, i)	xa_load(xa, i)
#define	radix_tree_delete(xa, i)	xa_erase(xa, i)
#define	radix_tree_empty(xa)		xa_empty(xa)
/*
 * Slot iteration over the backing XArray: *slot is a copy of the entry
 * (iter->entry), not a pointer into the tree; radix_tree_iter_delete()
 * erases the entry at iter->index. Enough for lookups and deletion while
 * iterating, which is what DRM/i915 do.
 */
struct radix_tree_iter {
	unsigned long	index;
	unsigned long	next_index;
	void		*entry;
};

#define	radix_tree_for_each_slot(slot, root, iter, start)		\
	for ((iter)->index = (start);					\
	    ((iter)->entry = xa_find((root), &(iter)->index, ULONG_MAX,	\
	    XA_PRESENT)) != NULL && ((slot) = &(iter)->entry, 1);	\
	    (iter)->index++)
#define	radix_tree_deref_slot(slot)		(*(slot))
#define	radix_tree_deref_slot_protected(slot, l)	(*(slot))
#define	radix_tree_iter_delete(root, iter, slot)			\
	((void)(slot), (void)xa_erase((root), (iter)->index))
#define	radix_tree_iter_resume(slot, iter)	(slot)

#define	radix_tree_preload(gfp)		(0)
#define	radix_tree_maybe_preload(gfp)	(0)
#define	radix_tree_preload_end()	do { } while (0)

#endif /* _LINUXKPI_INCLUDE_LINUX_RADIX_TREE_H_ */
