/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation of the Linux IDR and IDA APIs from their
 * documented behaviour (Documentation/core-api/idr.rst), built on the
 * XArray (<linux/xarray.h>) as in Linux 6.x (src/linux_idr.c).
 *
 * The IDR has its own lock (the XArray's); callers that also need their
 * own lock may keep it. idr_preload()/idr_preload_end() are no-ops: an
 * allocation with GFP_NOWAIT after idr_preload() can fail under memory
 * pressure here, where Linux would have used the preloaded node.
 */
#ifndef _LINUXKPI_LINUX_IDR_H_
#define	_LINUXKPI_LINUX_IDR_H_

#include <linuxkpi/platform.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/gfp.h>
#include <linux/err.h>
#include <linux/xarray.h>

struct idr {
	struct xarray	idr_rt;
	unsigned int	idr_base;
	unsigned int	idr_next;
};

#define	IDR_INIT_BASE(name, base)					\
	{ .idr_rt = XARRAY_INIT(name.idr_rt, XA_FLAGS_ALLOC),		\
	    .idr_base = (base), .idr_next = 0 }
#define	IDR_INIT(name)		IDR_INIT_BASE(name, 0)
#define	DEFINE_IDR(name)	struct idr name = IDR_INIT(name)

static inline void
idr_init_base(struct idr *idr, int base)
{
	xa_init_flags(&idr->idr_rt, XA_FLAGS_ALLOC);
	idr->idr_base = (unsigned int)base;
	idr->idr_next = 0;
}

static inline void
idr_init(struct idr *idr)
{
	idr_init_base(idr, 0);
}

static inline bool
idr_is_empty(const struct idr *idr)
{
	return (xa_empty(&idr->idr_rt));
}

static inline unsigned int
idr_get_cursor(const struct idr *idr)
{
	return (READ_ONCE(idr->idr_next));
}

static inline void
idr_set_cursor(struct idr *idr, unsigned int val)
{
	WRITE_ONCE(idr->idr_next, val);
}

#define	idr_lock(idr)		xa_lock(&(idr)->idr_rt)
#define	idr_unlock(idr)		xa_unlock(&(idr)->idr_rt)
#define	idr_lock_irq(idr)	xa_lock_irq(&(idr)->idr_rt)
#define	idr_unlock_irq(idr)	xa_unlock_irq(&(idr)->idr_rt)
#define	idr_lock_irqsave(idr, f) xa_lock_irqsave(&(idr)->idr_rt, f)
#define	idr_unlock_irqrestore(idr, f) xa_unlock_irqrestore(&(idr)->idr_rt, f)

#define	idr_preload(gfp)	do { (void)(gfp); } while (0)
#define	idr_preload_end()	do { } while (0)

/*
 * Allocate an id in [start, end) (end <= 0: up to INT_MAX) for ptr (NULL
 * reserves the id). Returns the id, -ENOSPC or -ENOMEM.
 */
int idr_alloc(struct idr *idr, void *ptr, int start, int end, gfp_t gfp);
/* As idr_alloc(), starting after the last allocated id and wrapping. */
int idr_alloc_cyclic(struct idr *idr, void *ptr, int start, int end,
    gfp_t gfp);
/* Allocate in [*nextid, max]; 0 and *nextid = id, or -ENOSPC/-ENOMEM. */
int idr_alloc_u32(struct idr *idr, void *ptr, u32 *nextid,
    unsigned long max, gfp_t gfp);
void *idr_find(const struct idr *idr, unsigned long id);
void *idr_remove(struct idr *idr, unsigned long id);
/* Replace the pointer at id; returns the old one or ERR_PTR(-ENOENT). */
void *idr_replace(struct idr *idr, void *ptr, unsigned long id);
int idr_for_each(const struct idr *idr,
    int (*fn)(int id, void *p, void *data), void *data);
void *idr_get_next(const struct idr *idr, int *nextid);
void *idr_get_next_ul(const struct idr *idr, unsigned long *nextid);
void idr_destroy(struct idr *idr);

#define	idr_for_each_entry(idr, entry, id)				\
	for ((id) = 0; ((entry) = idr_get_next((idr), &(id))) != NULL;	\
	    (id) += 1U)
#define	idr_for_each_entry_ul(idr, entry, tmp, id)			\
	for ((tmp) = 0, (id) = 0;					\
	    ((entry) = idr_get_next_ul((idr), &(id))) != NULL;		\
	    (tmp) = (id), (id) += 1UL)
#define	idr_for_each_entry_continue(idr, entry, id)			\
	for ((entry) = idr_get_next((idr), &(id)); (entry) != NULL;	\
	    (id) += 1U, (entry) = idr_get_next((idr), &(id)))

/* IDA: allocator of bare ids (no pointer). */
struct ida {
	struct xarray	xa;
};

#define	IDA_INIT_FLAGS		XA_FLAGS_ALLOC
#define	IDA_INIT(name)		{ .xa = XARRAY_INIT(name.xa, IDA_INIT_FLAGS) }
#define	DEFINE_IDA(name)	struct ida name = IDA_INIT(name)

static inline void
ida_init(struct ida *ida)
{
	xa_init_flags(&ida->xa, IDA_INIT_FLAGS);
}

static inline bool
ida_is_empty(const struct ida *ida)
{
	return (xa_empty(&ida->xa));
}

/* Lowest free id in [min, max]; -ENOSPC or -ENOMEM on failure. */
int ida_alloc_range(struct ida *ida, unsigned int min, unsigned int max,
    gfp_t gfp);
void ida_free(struct ida *ida, unsigned int id);
void ida_destroy(struct ida *ida);

static inline int
ida_alloc(struct ida *ida, gfp_t gfp)
{
	return (ida_alloc_range(ida, 0, INT_MAX, gfp));
}

static inline int
ida_alloc_min(struct ida *ida, unsigned int min, gfp_t gfp)
{
	return (ida_alloc_range(ida, min, INT_MAX, gfp));
}

static inline int
ida_alloc_max(struct ida *ida, unsigned int max, gfp_t gfp)
{
	return (ida_alloc_range(ida, 0, max, gfp));
}

/* Old interface: [start, end), end == 0 meaning no limit. */
static inline int
ida_simple_get(struct ida *ida, unsigned int start, unsigned int end,
    gfp_t gfp)
{
	return (ida_alloc_range(ida, start, end != 0 ? end - 1 : INT_MAX,
	    gfp));
}

#define	ida_simple_remove(ida, id)	ida_free(ida, id)

#endif /* _LINUXKPI_LINUX_IDR_H_ */
