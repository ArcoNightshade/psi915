/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * XArray basic API (see <linux/xarray.h>). Clean-room design: a radix tree
 * of 64-slot nodes. A node at height shift covers 64 << shift indices;
 * leaves have shift 0 and hold entries, other nodes hold child nodes.
 * xa_head points at the root node (or is NULL when the array is empty).
 *
 * Each node keeps the number of non-NULL slots (count, used to free empty
 * nodes) and a "full" bitmap: bit i is set when slot i is a non-NULL entry
 * (leaf) or a completely full child (inner node). xa_alloc() uses it to
 * skip full subtrees. Reserved entries (XA_ZERO_ENTRY) count as full.
 *
 * Search marks: marks[m] bit i is set when slot i holds an entry with
 * mark m (leaf) or a child with any entry carrying mark m (inner node).
 * Erasing an entry clears its marks; replacing it keeps them.
 */

#include <linux/kernel.h>
#include <linux/xarray.h>
#include <linux/slab.h>

#define	XA_CHUNK_SHIFT	6
#define	XA_CHUNK_SIZE	(1U << XA_CHUNK_SHIFT)
#define	XA_CHUNK_MASK	(XA_CHUNK_SIZE - 1)
#define	XA_FULL		(~0UL)
#define	XA_MARK_COUNT	3

struct lkpi_xa_node {
	unsigned char		shift;
	unsigned char		offset;
	unsigned short		count;
	struct lkpi_xa_node	*parent;
	unsigned long		full;
	unsigned long		marks[XA_MARK_COUNT];
	void			*slots[XA_CHUNK_SIZE];
};

static inline void *
lkpi_xa_zero_to_null(void *entry)
{
	return (xa_is_zero(entry) ? NULL : entry);
}

/* Largest index a root of this shift covers. */
static inline unsigned long
lkpi_xa_node_max(const struct lkpi_xa_node *n)
{
	unsigned int bits = n->shift + XA_CHUNK_SHIFT;

	return (bits >= BITS_PER_LONG ? ULONG_MAX : (1UL << bits) - 1);
}

/* Take xa_lock unless the caller holds it; returns whether we took it. */
static inline bool
lkpi_xa_lock(struct xarray *xa)
{
	if (lkpi_spin_held(&xa->xa_lock))
		return (false);
	spin_lock(&xa->xa_lock);
	return (true);
}

static inline void
lkpi_xa_unlock(struct xarray *xa, bool locked)
{
	if (locked)
		spin_unlock(&xa->xa_lock);
}

static struct lkpi_xa_node *
lkpi_xa_node_alloc(unsigned int shift, struct lkpi_xa_node *parent,
    unsigned int offset, gfp_t gfp)
{
	struct lkpi_xa_node *n;

	n = kzalloc(sizeof (*n), (gfp & ~__GFP_ZERO) | __GFP_NOWARN);
	if (n == NULL)
		return (NULL);
	n->shift = (unsigned char)shift;
	n->offset = (unsigned char)offset;
	n->parent = parent;
	return (n);
}

/* Set or clear bit off of node's full map and propagate upwards. */
static void
lkpi_xa_update_full(struct lkpi_xa_node *n, unsigned int off, bool full)
{
	bool was, is;

	while (n != NULL) {
		was = n->full == XA_FULL;
		if (full)
			n->full |= 1UL << off;
		else
			n->full &= ~(1UL << off);
		is = n->full == XA_FULL;
		if (was == is)
			return;
		off = n->offset;
		full = is;
		n = n->parent;
	}
}

/* Set or clear mark bit off of node n and propagate upwards. */
static void
lkpi_xa_update_mark(struct lkpi_xa_node *n, unsigned int off, unsigned int m,
    bool set)
{
	bool was, is;

	while (n != NULL) {
		was = n->marks[m] != 0;
		if (set)
			n->marks[m] |= 1UL << off;
		else
			n->marks[m] &= ~(1UL << off);
		is = n->marks[m] != 0;
		if (was == is)
			return;
		off = n->offset;
		set = is;
		n = n->parent;
	}
}

static void
lkpi_xa_clear_marks(struct lkpi_xa_node *leaf, unsigned int off)
{
	unsigned int m;

	for (m = 0; m < XA_MARK_COUNT; m++)
		if (leaf->marks[m] & (1UL << off))
			lkpi_xa_update_mark(leaf, off, m, false);
}

/* Free empty nodes from n upwards, then drop useless root levels. */
static void
lkpi_xa_prune(struct xarray *xa, struct lkpi_xa_node *n)
{
	struct lkpi_xa_node *parent, *root, *child;

	while (n != NULL && n->count == 0) {
		parent = n->parent;
		if (parent != NULL) {
			parent->slots[n->offset] = NULL;
			parent->count--;
			lkpi_xa_update_full(parent, n->offset, false);
		} else {
			xa->xa_head = NULL;
		}
		kfree(n);
		n = parent;
	}
	/* A root whose only child is slot 0 can be replaced by that child. */
	while ((root = xa->xa_head) != NULL && root->shift > 0 &&
	    root->count == 1 && root->slots[0] != NULL) {
		child = root->slots[0];
		child->parent = NULL;
		child->offset = 0;
		xa->xa_head = child;
		kfree(root);
	}
}

/* Make the tree tall enough to hold index. */
static int
lkpi_xa_grow(struct xarray *xa, unsigned long index, gfp_t gfp)
{
	struct lkpi_xa_node *root = xa->xa_head, *n;
	unsigned int shift = 0;

	if (root == NULL) {
		while (shift + XA_CHUNK_SHIFT < BITS_PER_LONG &&
		    index > (1UL << (shift + XA_CHUNK_SHIFT)) - 1)
			shift += XA_CHUNK_SHIFT;
		n = lkpi_xa_node_alloc(shift, NULL, 0, gfp);
		if (n == NULL)
			return (-ENOMEM);
		xa->xa_head = n;
		return (0);
	}
	while (index > lkpi_xa_node_max(root)) {
		n = lkpi_xa_node_alloc(root->shift + XA_CHUNK_SHIFT, NULL, 0,
		    gfp);
		if (n == NULL)
			return (-ENOMEM);
		n->slots[0] = root;
		n->count = 1;
		if (root->full == XA_FULL)
			n->full = 1;
		for (unsigned int m = 0; m < XA_MARK_COUNT; m++)
			if (root->marks[m] != 0)
				n->marks[m] = 1;
		root->parent = n;
		root->offset = 0;
		xa->xa_head = n;
		root = n;
	}
	return (0);
}

/* Leaf holding index (created if create), or NULL. */
static struct lkpi_xa_node *
lkpi_xa_leaf(struct xarray *xa, unsigned long index, bool create, gfp_t gfp)
{
	struct lkpi_xa_node *n, *child;
	unsigned int i;

	if (create && lkpi_xa_grow(xa, index, gfp) != 0)
		return (NULL);
	n = xa->xa_head;
	if (n == NULL || index > lkpi_xa_node_max(n))
		return (NULL);
	while (n->shift > 0) {
		i = (index >> n->shift) & XA_CHUNK_MASK;
		child = n->slots[i];
		if (child == NULL) {
			if (!create)
				return (NULL);
			child = lkpi_xa_node_alloc(n->shift - XA_CHUNK_SHIFT,
			    n, i, gfp);
			if (child == NULL) {
				lkpi_xa_prune(xa, n);
				return (NULL);
			}
			n->slots[i] = child;
			n->count++;
		}
		n = child;
	}
	return (n);
}

/* Raw entry at index; caller holds xa_lock. */
static void *
lkpi_xa_get(struct xarray *xa, unsigned long index)
{
	struct lkpi_xa_node *leaf = lkpi_xa_leaf(xa, index, false, 0);

	return (leaf != NULL ? leaf->slots[index & XA_CHUNK_MASK] : NULL);
}

void *
lkpi_xa_load_raw(struct xarray *xa, unsigned long index)
{
	bool locked = lkpi_xa_lock(xa);
	void *entry = lkpi_xa_get(xa, index);

	lkpi_xa_unlock(xa, locked);
	return (entry);
}

void *
xa_load(struct xarray *xa, unsigned long index)
{
	return (lkpi_xa_zero_to_null(lkpi_xa_load_raw(xa, index)));
}

/* Store a raw entry (NULL erases); returns the old raw entry. */
static void *
lkpi_xa_store_raw(struct xarray *xa, unsigned long index, void *entry,
    gfp_t gfp)
{
	struct lkpi_xa_node *leaf;
	unsigned int off = index & XA_CHUNK_MASK;
	void *old;

	leaf = lkpi_xa_leaf(xa, index, entry != NULL, gfp);
	if (leaf == NULL)
		return (entry != NULL ? XA_ERROR(-ENOMEM) : NULL);
	old = leaf->slots[off];
	leaf->slots[off] = entry;
	if (old == NULL && entry != NULL) {
		leaf->count++;
		lkpi_xa_update_full(leaf, off, true);
	} else if (old != NULL && xa_is_zero(entry)) {
		lkpi_xa_clear_marks(leaf, off);
	} else if (old != NULL && entry == NULL) {
		lkpi_xa_clear_marks(leaf, off);
		leaf->count--;
		lkpi_xa_update_full(leaf, off, false);
		lkpi_xa_prune(xa, leaf);
	}
	return (old);
}

void *
__xa_store(struct xarray *xa, unsigned long index, void *entry, gfp_t gfp)
{
	void *old;

	if (xa_is_internal(entry) && !xa_is_zero(entry))
		return (XA_ERROR(-EINVAL));
	if (entry == NULL && (xa->xa_flags & XA_FLAGS_TRACK_FREE))
		entry = XA_ZERO_ENTRY;
	old = lkpi_xa_store_raw(xa, index, entry, gfp);
	return (xa_is_err(old) ? old : lkpi_xa_zero_to_null(old));
}

void *
xa_store(struct xarray *xa, unsigned long index, void *entry, gfp_t gfp)
{
	bool locked = lkpi_xa_lock(xa);
	void *old = __xa_store(xa, index, entry, gfp);

	lkpi_xa_unlock(xa, locked);
	return (old);
}

void *
__xa_erase(struct xarray *xa, unsigned long index)
{
	return (lkpi_xa_zero_to_null(lkpi_xa_store_raw(xa, index, NULL, 0)));
}

void *
xa_erase(struct xarray *xa, unsigned long index)
{
	bool locked = lkpi_xa_lock(xa);
	void *old = __xa_erase(xa, index);

	lkpi_xa_unlock(xa, locked);
	return (old);
}

/*
 * Replace the entry at index with entry if it currently is old (a
 * reserved entry matches old == NULL); returns the entry found.
 * old == XA_ZERO_ENTRY matches only a reserved entry and, with entry ==
 * NULL, really erases it (xa_release()).
 */
void *
__xa_cmpxchg(struct xarray *xa, unsigned long index, void *old, void *entry,
    gfp_t gfp)
{
	void *cur, *r;

	if (xa_is_internal(entry) && !xa_is_zero(entry))
		return (XA_ERROR(-EINVAL));
	cur = lkpi_xa_get(xa, index);
	if (cur == old || (old == NULL && xa_is_zero(cur))) {
		if (entry == NULL && !xa_is_zero(old) &&
		    (xa->xa_flags & XA_FLAGS_TRACK_FREE))
			entry = XA_ZERO_ENTRY;
		if (entry != cur) {
			r = lkpi_xa_store_raw(xa, index, entry, gfp);
			if (xa_is_err(r))
				return (r);
		}
	}
	return (lkpi_xa_zero_to_null(cur));
}

void *
xa_cmpxchg(struct xarray *xa, unsigned long index, void *old, void *entry,
    gfp_t gfp)
{
	bool locked = lkpi_xa_lock(xa);
	void *r = __xa_cmpxchg(xa, index, old, entry, gfp);

	lkpi_xa_unlock(xa, locked);
	return (r);
}

int
__xa_insert(struct xarray *xa, unsigned long index, void *entry, gfp_t gfp)
{
	void *r;

	if (xa_is_internal(entry) && !xa_is_zero(entry))
		return (-EINVAL);
	if (entry == NULL)
		entry = XA_ZERO_ENTRY;
	if (lkpi_xa_get(xa, index) != NULL)
		return (-EBUSY);
	r = lkpi_xa_store_raw(xa, index, entry, gfp);
	return (xa_is_err(r) ? xa_err(r) : 0);
}

int
xa_insert(struct xarray *xa, unsigned long index, void *entry, gfp_t gfp)
{
	bool locked = lkpi_xa_lock(xa);
	int r = __xa_insert(xa, index, entry, gfp);

	lkpi_xa_unlock(xa, locked);
	return (r);
}

int
xa_reserve(struct xarray *xa, unsigned long index, gfp_t gfp)
{
	bool locked = lkpi_xa_lock(xa);
	void *r;
	int err = 0;

	if (lkpi_xa_get(xa, index) == NULL) {
		r = lkpi_xa_store_raw(xa, index, XA_ZERO_ENTRY, gfp);
		err = xa_err(r);
	}
	lkpi_xa_unlock(xa, locked);
	return (err);
}

/*
 * First index in [start, max] under n (covering base..) whose slot
 * passes: present entries (find) or free slots (alloc).
 */
static bool
lkpi_xa_find_in(const struct lkpi_xa_node *n, unsigned long base,
    unsigned long start, unsigned long max, bool want_free, int mark,
    unsigned long *found, void **entryp)
{
	unsigned int s = n->shift, i;
	unsigned long slot_base;
	void *e;

	i = start > base ? (unsigned int)((start - base) >> s) : 0;
	for (; i < XA_CHUNK_SIZE; i++) {
		if ((unsigned long)i > (ULONG_MAX - base) >> s)
			break;
		slot_base = base + ((unsigned long)i << s);
		if (slot_base > max)
			break;
		e = n->slots[i];
		if (want_free) {
			if (n->full & (1UL << i))
				continue;
			if (e == NULL) {
				*found = start > slot_base ? start : slot_base;
				return (true);
			}
		} else if (e == NULL) {
			continue;
		} else if (mark >= 0 && !(n->marks[mark] & (1UL << i))) {
			continue;
		}
		if (s == 0) {
			if (want_free || xa_is_zero(e))
				continue;
			*found = slot_base;
			*entryp = e;
			return (true);
		}
		if (lkpi_xa_find_in(e, slot_base,
		    start > slot_base ? start : slot_base, max, want_free,
		    mark, found, entryp))
			return (true);
	}
	return (false);
}

static void *
lkpi_xa_find_locked(struct xarray *xa, unsigned long start,
    unsigned long max, xa_mark_t filter, unsigned long *found)
{
	void *entry = NULL;
	int mark = filter < XA_MARK_COUNT ? (int)filter : -1;

	if (xa->xa_head == NULL || start > max)
		return (NULL);
	if (lkpi_xa_find_in(xa->xa_head, 0, start, max, false, mark, found,
	    &entry))
		return (entry);
	return (NULL);
}

void *
xa_find(struct xarray *xa, unsigned long *index, unsigned long max,
    xa_mark_t filter)
{
	bool locked = lkpi_xa_lock(xa);
	unsigned long found = 0;
	void *entry;

	entry = lkpi_xa_find_locked(xa, *index, max, filter, &found);
	if (entry != NULL)
		*index = found;
	lkpi_xa_unlock(xa, locked);
	return (entry);
}

void *
xa_find_after(struct xarray *xa, unsigned long *index, unsigned long max,
    xa_mark_t filter)
{
	unsigned long next;
	void *entry;

	if (*index == ULONG_MAX)
		return (NULL);
	next = *index + 1;
	entry = xa_find(xa, &next, max, filter);
	if (entry != NULL)
		*index = next;
	return (entry);
}

/* Lowest free index in [min, max]. */
static bool
lkpi_xa_find_free(struct xarray *xa, unsigned long min, unsigned long max,
    unsigned long *found)
{
	struct lkpi_xa_node *root = xa->xa_head;
	unsigned long rmax;

	if (min > max)
		return (false);
	if (root == NULL) {
		*found = min;
		return (true);
	}
	if (lkpi_xa_find_in(root, 0, min, max, true, -1, found, NULL))
		return (true);
	rmax = lkpi_xa_node_max(root);
	if (rmax < max) {
		*found = min > rmax ? min : rmax + 1;
		return (true);
	}
	return (false);
}

int
__xa_alloc(struct xarray *xa, u32 *id, void *entry, struct xa_limit limit,
    gfp_t gfp)
{
	unsigned long idx, min = limit.min;
	void *r;

	if (xa_is_internal(entry) && !xa_is_zero(entry))
		return (-EINVAL);
	if ((xa->xa_flags & XA_FLAGS_ZERO_BUSY) && min == 0)
		min = 1;
	if (!lkpi_xa_find_free(xa, min, limit.max, &idx))
		return (-EBUSY);
	if (entry == NULL)
		entry = XA_ZERO_ENTRY;
	r = lkpi_xa_store_raw(xa, idx, entry, gfp);
	if (xa_is_err(r))
		return (xa_err(r));
	*id = (u32)idx;
	return (0);
}

int
xa_alloc(struct xarray *xa, u32 *id, void *entry, struct xa_limit limit,
    gfp_t gfp)
{
	bool locked = lkpi_xa_lock(xa);
	int r = __xa_alloc(xa, id, entry, limit, gfp);

	lkpi_xa_unlock(xa, locked);
	return (r);
}

/*
 * Allocate from *next upwards, wrapping to the bottom of the limit once.
 * Returns 0, 1 for the first allocation after the ids wrapped, or a
 * negative errno.
 */
int
__xa_alloc_cyclic(struct xarray *xa, u32 *id, void *entry,
    struct xa_limit limit, u32 *next, gfp_t gfp)
{
	u32 min = limit.min;
	struct xa_limit l = limit;
	bool wrapped = false;
	int r;

	if ((xa->xa_flags & XA_FLAGS_ZERO_BUSY) && min == 0)
		min = 1;
	l.min = *next > min ? *next : min;
	if (l.min > limit.max)
		l.min = min;
	r = __xa_alloc(xa, id, entry, l, gfp);
	if (r == -EBUSY && l.min > min) {
		l.min = min;
		r = __xa_alloc(xa, id, entry, l, gfp);
		wrapped = true;
	}
	if (r != 0)
		return (r);
	/* Report a wrap (this search's, or *next's last time) once. */
	if (wrapped)
		xa->xa_flags |= XA_FLAGS_ALLOC_WRAPPED;
	r = (xa->xa_flags & XA_FLAGS_ALLOC_WRAPPED) ? 1 : 0;
	xa->xa_flags &= ~XA_FLAGS_ALLOC_WRAPPED;
	if (*id == limit.max) {
		*next = min;
		xa->xa_flags |= XA_FLAGS_ALLOC_WRAPPED;
	} else {
		*next = *id + 1;
	}
	return (r);
}

int
xa_alloc_cyclic(struct xarray *xa, u32 *id, void *entry,
    struct xa_limit limit, u32 *next, gfp_t gfp)
{
	bool locked = lkpi_xa_lock(xa);
	int r = __xa_alloc_cyclic(xa, id, entry, limit, next, gfp);

	lkpi_xa_unlock(xa, locked);
	return (r);
}

static void
lkpi_xa_free_tree(struct lkpi_xa_node *n)
{
	unsigned int i;

	if (n->shift > 0)
		for (i = 0; i < XA_CHUNK_SIZE; i++)
			if (n->slots[i] != NULL)
				lkpi_xa_free_tree(n->slots[i]);
	kfree(n);
}

/* Free the tree; the entries themselves belong to the caller. */
void
xa_destroy(struct xarray *xa)
{
	bool locked = lkpi_xa_lock(xa);

	if (xa->xa_head != NULL)
		lkpi_xa_free_tree(xa->xa_head);
	xa->xa_head = NULL;
	lkpi_xa_unlock(xa, locked);
}

/* Marks. Only present (non-NULL, non-reserved) entries can be marked. */

void
__xa_set_mark(struct xarray *xa, unsigned long index, xa_mark_t mark)
{
	struct lkpi_xa_node *leaf;
	unsigned int off = index & XA_CHUNK_MASK;
	void *e;

	if (mark >= XA_MARK_COUNT)
		return;
	leaf = lkpi_xa_leaf(xa, index, false, 0);
	if (leaf == NULL)
		return;
	e = leaf->slots[off];
	if (e == NULL || xa_is_zero(e))
		return;
	if (!(leaf->marks[mark] & (1UL << off)))
		lkpi_xa_update_mark(leaf, off, mark, true);
}

void
__xa_clear_mark(struct xarray *xa, unsigned long index, xa_mark_t mark)
{
	struct lkpi_xa_node *leaf;
	unsigned int off = index & XA_CHUNK_MASK;

	if (mark >= XA_MARK_COUNT)
		return;
	leaf = lkpi_xa_leaf(xa, index, false, 0);
	if (leaf != NULL && (leaf->marks[mark] & (1UL << off)))
		lkpi_xa_update_mark(leaf, off, mark, false);
}

void
xa_set_mark(struct xarray *xa, unsigned long index, xa_mark_t mark)
{
	bool locked = lkpi_xa_lock(xa);

	__xa_set_mark(xa, index, mark);
	lkpi_xa_unlock(xa, locked);
}

void
xa_clear_mark(struct xarray *xa, unsigned long index, xa_mark_t mark)
{
	bool locked = lkpi_xa_lock(xa);

	__xa_clear_mark(xa, index, mark);
	lkpi_xa_unlock(xa, locked);
}

bool
xa_get_mark(struct xarray *xa, unsigned long index, xa_mark_t mark)
{
	bool locked = lkpi_xa_lock(xa);
	struct lkpi_xa_node *leaf;
	bool r = false;

	if (mark < XA_MARK_COUNT) {
		leaf = lkpi_xa_leaf(xa, index, false, 0);
		r = leaf != NULL &&
		    (leaf->marks[mark] & (1UL << (index & XA_CHUNK_MASK)));
	}
	lkpi_xa_unlock(xa, locked);
	return (r);
}

bool
xa_marked(struct xarray *xa, xa_mark_t mark)
{
	bool locked = lkpi_xa_lock(xa);
	struct lkpi_xa_node *root = xa->xa_head;
	bool r = mark < XA_MARK_COUNT && root != NULL &&
	    root->marks[mark] != 0;

	lkpi_xa_unlock(xa, locked);
	return (r);
}
