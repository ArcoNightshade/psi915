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
#define	radix_tree_preload(gfp)		(0)
#define	radix_tree_maybe_preload(gfp)	(0)
#define	radix_tree_preload_end()	do { } while (0)

#endif /* _LINUXKPI_INCLUDE_LINUX_RADIX_TREE_H_ */
