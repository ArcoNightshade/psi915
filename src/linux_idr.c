/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * IDR and IDA over the XArray (see <linux/idr.h>). Clean-room.
 * IDR ids are stored at index (id - idr_base).
 */

#include <linux/kernel.h>
#include <linux/idr.h>
#include <linux/bug.h>

static int
lkpi_idr_alloc_range(struct idr *idr, void *ptr, unsigned long min,
    unsigned long max, gfp_t gfp, u32 *out)
{
	u32 id;
	int r;

	if (min < idr->idr_base)
		min = idr->idr_base;
	if (min > max || max - idr->idr_base > UINT_MAX)
		return (min > max ? -ENOSPC : -EINVAL);
	r = xa_alloc(&idr->idr_rt, &id, ptr,
	    XA_LIMIT((u32)(min - idr->idr_base), (u32)(max - idr->idr_base)),
	    gfp);
	if (r == -EBUSY)
		return (-ENOSPC);
	if (r != 0)
		return (r);
	*out = id + idr->idr_base;
	return (0);
}

int
idr_alloc(struct idr *idr, void *ptr, int start, int end, gfp_t gfp)
{
	u32 id;
	int r;

	if (WARN_ON_ONCE(start < 0))
		return (-EINVAL);
	r = lkpi_idr_alloc_range(idr, ptr, (unsigned long)start,
	    end > 0 ? (unsigned long)end - 1 : INT_MAX, gfp, &id);
	return (r != 0 ? r : (int)id);
}

int
idr_alloc_u32(struct idr *idr, void *ptr, u32 *nextid, unsigned long max,
    gfp_t gfp)
{
	if (max > UINT_MAX)
		max = UINT_MAX;
	return (lkpi_idr_alloc_range(idr, ptr, *nextid, max, gfp, nextid));
}

int
idr_alloc_cyclic(struct idr *idr, void *ptr, int start, int end, gfp_t gfp)
{
	unsigned long max = end > 0 ? (unsigned long)end - 1 : INT_MAX;
	unsigned long from = idr->idr_next > (unsigned int)start ?
	    idr->idr_next : (unsigned long)start;
	u32 id;
	int r;

	if (WARN_ON_ONCE(start < 0))
		return (-EINVAL);
	r = lkpi_idr_alloc_range(idr, ptr, from, max, gfp, &id);
	if (r == -ENOSPC && from > (unsigned long)start)
		r = lkpi_idr_alloc_range(idr, ptr, (unsigned long)start, max,
		    gfp, &id);
	if (r != 0)
		return (r);
	idr->idr_next = id + 1;
	return ((int)id);
}

void *
idr_find(const struct idr *idr, unsigned long id)
{
	if (id < idr->idr_base)
		return (NULL);
	return (xa_load((struct xarray *)(uintptr_t)&idr->idr_rt,
	    id - idr->idr_base));
}

void *
idr_remove(struct idr *idr, unsigned long id)
{
	if (id < idr->idr_base)
		return (NULL);
	return (xa_erase(&idr->idr_rt, id - idr->idr_base));
}

void *
idr_replace(struct idr *idr, void *ptr, unsigned long id)
{
	struct xarray *xa = &idr->idr_rt;
	unsigned long idx;
	void *old;

	if (id < idr->idr_base)
		return (ERR_PTR(-ENOENT));
	idx = id - idr->idr_base;
	xa_lock(xa);
	old = lkpi_xa_load_raw(xa, idx);
	if (old == NULL) {
		xa_unlock(xa);
		return (ERR_PTR(-ENOENT));
	}
	(void) __xa_store(xa, idx, ptr, 0);	/* slot exists: no allocation */
	xa_unlock(xa);
	return (xa_is_zero(old) ? NULL : old);
}

void *
idr_get_next_ul(const struct idr *idr, unsigned long *nextid)
{
	struct xarray *xa = (struct xarray *)(uintptr_t)&idr->idr_rt;
	unsigned long idx;
	void *entry;

	idx = *nextid > idr->idr_base ? *nextid - idr->idr_base : 0;
	entry = xa_find(xa, &idx, ULONG_MAX, XA_PRESENT);
	if (entry != NULL)
		*nextid = idx + idr->idr_base;
	return (entry);
}

void *
idr_get_next(const struct idr *idr, int *nextid)
{
	unsigned long id = (unsigned long)*nextid;
	void *entry = idr_get_next_ul(idr, &id);

	if (entry == NULL || WARN_ON_ONCE(id > INT_MAX))
		return (NULL);
	*nextid = (int)id;
	return (entry);
}

/* Calls fn for each id in order; stops at the first non-zero result. */
int
idr_for_each(const struct idr *idr, int (*fn)(int id, void *p, void *data),
    void *data)
{
	void *entry;
	int id, r;

	for (id = 0; (entry = idr_get_next(idr, &id)) != NULL; id++) {
		r = fn(id, entry, data);
		if (r != 0)
			return (r);
	}
	return (0);
}

void
idr_destroy(struct idr *idr)
{
	xa_destroy(&idr->idr_rt);
}

int
ida_alloc_range(struct ida *ida, unsigned int min, unsigned int max,
    gfp_t gfp)
{
	u32 id;
	int r;

	if ((int)min < 0)
		return (-ENOSPC);
	if (max > INT_MAX)
		max = INT_MAX;
	r = xa_alloc(&ida->xa, &id, xa_mk_value(1), XA_LIMIT(min, max), gfp);
	if (r == -EBUSY)
		return (-ENOSPC);
	return (r != 0 ? r : (int)id);
}

void
ida_free(struct ida *ida, unsigned int id)
{
	void *old = xa_erase(&ida->xa, id);

	WARN(old == NULL, "linuxkpi: ida_free(%u): id not allocated\n", id);
}

void
ida_destroy(struct ida *ida)
{
	xa_destroy(&ida->xa);
}
