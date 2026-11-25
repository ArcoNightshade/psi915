/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation of the basic Linux XArray API from its
 * documented behaviour (Documentation/core-api/xarray.rst); function set
 * cross-checked against FreeBSD's LinuxKPI xarray.h (BSD-2-Clause).
 *
 * Implementation (src/linux_xarray.c): a 64-way radix tree of nodes, with
 * a per-node "full" bitmap so xa_alloc() skips full subtrees. Every
 * operation takes xa_lock (a spinlock_t) unless the caller already holds
 * it, so xa_load() is not lockless as in Linux but may still be called
 * with or without the lock held. The __xa_* forms require the lock.
 *
 * Entries: pointers (4-byte aligned) and value entries (xa_mk_value()).
 * Storing NULL erases, except in allocating arrays (XA_FLAGS_ALLOC*),
 * where it reserves the index like xa_reserve(): reserved indices read as
 * NULL, are skipped by iteration, and are not handed out by xa_alloc().
 *
 * Search marks XA_MARK_0..2 (xa_set_mark(), xa_get_mark(),
 * xa_for_each_marked(), xa_find() with a mark filter) are kept per node
 * and propagated up the tree; erasing an entry clears its marks. Unlike
 * Linux, allocating arrays do not use XA_MARK_0 internally (free slots
 * are tracked separately), so all three marks are available to callers.
 *
 * Not provided yet: multi-index entries, the xa_state (xas_*) advanced
 * API.
 */
#ifndef _LINUXKPI_LINUX_XARRAY_H_
#define	_LINUXKPI_LINUX_XARRAY_H_

#include <linuxkpi/platform.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/limits.h>
#include <linux/gfp.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/spinlock.h>

#define	XA_FLAGS_LOCK_IRQ	((gfp_t)0x01000000)
#define	XA_FLAGS_LOCK_BH	((gfp_t)0x02000000)
#define	XA_FLAGS_TRACK_FREE	((gfp_t)0x04000000)
#define	XA_FLAGS_ZERO_BUSY	((gfp_t)0x08000000)
#define	XA_FLAGS_ALLOC_WRAPPED	((gfp_t)0x10000000)
#define	XA_FLAGS_ACCOUNT	((gfp_t)0x20000000)
#define	XA_FLAGS_ALLOC		(XA_FLAGS_TRACK_FREE)
#define	XA_FLAGS_ALLOC1		(XA_FLAGS_TRACK_FREE | XA_FLAGS_ZERO_BUSY)

struct xarray {
	spinlock_t	xa_lock;
	gfp_t		xa_flags;
	void		*xa_head;
};

#define	XARRAY_INIT(name, flags)					\
	{ .xa_lock = __SPIN_LOCK_UNLOCKED(name.xa_lock),		\
	    .xa_flags = (flags), .xa_head = NULL }
#define	DEFINE_XARRAY_FLAGS(name, flags)				\
	struct xarray name = XARRAY_INIT(name, flags)
#define	DEFINE_XARRAY(name)		DEFINE_XARRAY_FLAGS(name, 0)
#define	DEFINE_XARRAY_ALLOC(name)	DEFINE_XARRAY_FLAGS(name, XA_FLAGS_ALLOC)
#define	DEFINE_XARRAY_ALLOC1(name)	DEFINE_XARRAY_FLAGS(name, XA_FLAGS_ALLOC1)

struct xa_limit {
	u32	max;
	u32	min;
};

#define	XA_LIMIT(lo, hi)	((struct xa_limit){ .min = (lo), .max = (hi) })
#define	xa_limit_32b		XA_LIMIT(0, UINT_MAX)
#define	xa_limit_31b		XA_LIMIT(0, INT_MAX)
#define	xa_limit_16b		XA_LIMIT(0, USHRT_MAX)
#ifndef USHRT_MAX
#define	USHRT_MAX		0xffff
#endif

typedef unsigned int xa_mark_t;
#define	XA_MARK_0		((xa_mark_t)0U)
#define	XA_MARK_1		((xa_mark_t)1U)
#define	XA_MARK_2		((xa_mark_t)2U)
#define	XA_MARK_MAX		XA_MARK_2
#define	XA_FREE_MARK		XA_MARK_0
#define	XA_PRESENT		((xa_mark_t)8U)

/* Entry encoding: bit 0 set = value, bits 1:0 == 10 = internal. */
static inline void *
xa_mk_value(unsigned long v)
{
	return ((void *)((v << 1) | 1));
}

static inline unsigned long
xa_to_value(const void *entry)
{
	return ((unsigned long)entry >> 1);
}

static inline bool
xa_is_value(const void *entry)
{
	return (((unsigned long)entry & 1) != 0);
}

static inline void *
xa_mk_internal(unsigned long v)
{
	return ((void *)((v << 2) | 2));
}

static inline bool
xa_is_internal(const void *entry)
{
	return (((unsigned long)entry & 3) == 2);
}

#define	XA_ZERO_ENTRY		xa_mk_internal(257)
#define	XA_ERROR(errno)		((void *)(((unsigned long)(long)(errno) << 2) | 2UL))

static inline bool
xa_is_zero(const void *entry)
{
	return (entry == XA_ZERO_ENTRY);
}

static inline bool
xa_is_err(const void *entry)
{
	return (xa_is_internal(entry) &&
	    (unsigned long)entry >= (unsigned long)XA_ERROR(-MAX_ERRNO));
}

/* The negative errno an xa_store()/xa_cmpxchg() result encodes, or 0. */
static inline int
xa_err(const void *entry)
{
	return (xa_is_err(entry) ? (int)((long)entry >> 2) : 0);
}

static inline void *
xa_tag_pointer(void *p, unsigned long tag)
{
	return ((void *)((unsigned long)p | tag));
}

static inline void *
xa_untag_pointer(void *entry)
{
	return ((void *)((unsigned long)entry & ~3UL));
}

static inline unsigned int
xa_pointer_tag(void *entry)
{
	return ((unsigned int)((unsigned long)entry & 3UL));
}

static inline void
xa_init_flags(struct xarray *xa, gfp_t flags)
{
	spin_lock_init(&xa->xa_lock);
	xa->xa_flags = flags;
	xa->xa_head = NULL;
}

static inline void
xa_init(struct xarray *xa)
{
	xa_init_flags(xa, 0);
}

static inline bool
xa_empty(const struct xarray *xa)
{
	return (READ_ONCE(xa->xa_head) == NULL);
}

#define	xa_lock(xa)		spin_lock(&(xa)->xa_lock)
#define	xa_unlock(xa)		spin_unlock(&(xa)->xa_lock)
#define	xa_lock_bh(xa)		spin_lock_bh(&(xa)->xa_lock)
#define	xa_unlock_bh(xa)	spin_unlock_bh(&(xa)->xa_lock)
#define	xa_lock_irq(xa)		spin_lock_irq(&(xa)->xa_lock)
#define	xa_unlock_irq(xa)	spin_unlock_irq(&(xa)->xa_lock)
#define	xa_lock_irqsave(xa, f)	spin_lock_irqsave(&(xa)->xa_lock, f)
#define	xa_unlock_irqrestore(xa, f) spin_unlock_irqrestore(&(xa)->xa_lock, f)
#define	xa_trylock(xa)		spin_trylock(&(xa)->xa_lock)

void *xa_load(struct xarray *xa, unsigned long index);
void *xa_store(struct xarray *xa, unsigned long index, void *entry, gfp_t gfp);
void *xa_erase(struct xarray *xa, unsigned long index);
void *xa_cmpxchg(struct xarray *xa, unsigned long index, void *old,
    void *entry, gfp_t gfp);
int xa_insert(struct xarray *xa, unsigned long index, void *entry, gfp_t gfp);
int xa_alloc(struct xarray *xa, u32 *id, void *entry, struct xa_limit limit,
    gfp_t gfp);
int xa_alloc_cyclic(struct xarray *xa, u32 *id, void *entry,
    struct xa_limit limit, u32 *next, gfp_t gfp);
int xa_reserve(struct xarray *xa, unsigned long index, gfp_t gfp);
void xa_destroy(struct xarray *xa);
void *xa_find(struct xarray *xa, unsigned long *index, unsigned long max,
    xa_mark_t filter);
void *xa_find_after(struct xarray *xa, unsigned long *index,
    unsigned long max, xa_mark_t filter);

void *__xa_store(struct xarray *xa, unsigned long index, void *entry,
    gfp_t gfp);
void *__xa_erase(struct xarray *xa, unsigned long index);
void *__xa_cmpxchg(struct xarray *xa, unsigned long index, void *old,
    void *entry, gfp_t gfp);
int __xa_insert(struct xarray *xa, unsigned long index, void *entry,
    gfp_t gfp);
int __xa_alloc(struct xarray *xa, u32 *id, void *entry,
    struct xa_limit limit, gfp_t gfp);
int __xa_alloc_cyclic(struct xarray *xa, u32 *id, void *entry,
    struct xa_limit limit, u32 *next, gfp_t gfp);

void xa_set_mark(struct xarray *xa, unsigned long index, xa_mark_t mark);
void xa_clear_mark(struct xarray *xa, unsigned long index, xa_mark_t mark);
bool xa_get_mark(struct xarray *xa, unsigned long index, xa_mark_t mark);
bool xa_marked(struct xarray *xa, xa_mark_t mark);
void __xa_set_mark(struct xarray *xa, unsigned long index, xa_mark_t mark);
void __xa_clear_mark(struct xarray *xa, unsigned long index,
    xa_mark_t mark);

/* Entry at index including reserved entries (as XA_ZERO_ENTRY). */
void *lkpi_xa_load_raw(struct xarray *xa, unsigned long index);

static inline void
xa_release(struct xarray *xa, unsigned long index)
{
	(void) xa_cmpxchg(xa, index, XA_ZERO_ENTRY, NULL, 0);
}

#define	xa_store_irq(xa, i, e, g)	xa_store(xa, i, e, g)
#define	xa_store_bh(xa, i, e, g)	xa_store(xa, i, e, g)
#define	xa_erase_irq(xa, i)		xa_erase(xa, i)
#define	xa_erase_bh(xa, i)		xa_erase(xa, i)
#define	xa_alloc_irq(xa, id, e, l, g)	xa_alloc(xa, id, e, l, g)
#define	xa_alloc_bh(xa, id, e, l, g)	xa_alloc(xa, id, e, l, g)
#define	xa_insert_irq(xa, i, e, g)	xa_insert(xa, i, e, g)
#define	xa_insert_bh(xa, i, e, g)	xa_insert(xa, i, e, g)

/* Iterate present entries (reserved ones are skipped) in index order. */
#define	xa_for_each_range(xa, index, entry, start, last)		\
	for ((index) = (start),						\
	    (entry) = xa_find((xa), &(index), (last), XA_PRESENT);	\
	    (entry) != NULL;						\
	    (entry) = xa_find_after((xa), &(index), (last), XA_PRESENT))
#define	xa_for_each_start(xa, index, entry, start)			\
	xa_for_each_range(xa, index, entry, start, ULONG_MAX)
#define	xa_for_each(xa, index, entry)					\
	xa_for_each_start(xa, index, entry, 0)
#define	xa_for_each_marked(xa, index, entry, filter)			\
	for ((index) = 0,						\
	    (entry) = xa_find((xa), &(index), ULONG_MAX, (filter));	\
	    (entry) != NULL;						\
	    (entry) = xa_find_after((xa), &(index), ULONG_MAX, (filter)))

#endif /* _LINUXKPI_LINUX_XARRAY_H_ */
