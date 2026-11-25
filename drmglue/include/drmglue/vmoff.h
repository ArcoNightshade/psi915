/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * lkpi_vmoff: fake mmap offset manager with the semantics of Linux's
 * drm_vma_offset_manager (drm_vma_manager.h), written clean-room.
 *
 * Role (docs/10, section 3.2): the imported DRM core keeps using its own
 * drm_vma_manager.c (MIT) for GEM; nothing here replaces it. lkpi_vmoff
 * exists so that (a) the devmap/fault glue can be exercised end to end in
 * userland tests without the DRM import, and (b) bring-up can proceed if
 * drm_mm (which drm_vma_manager.c needs) is not yet ported. Its contract is
 * the one the glue relies on from either implementation:
 *
 *   - offsets are page granular; a node occupies [start, start + pages);
 *   - lookup(start, pages) returns the node that wholly contains the range,
 *     under the manager's read lock; the caller takes its object reference
 *     (kref_get_unless_zero) before dropping the lock;
 *   - per-node allow lists keyed by an opaque tag (the struct drm_file),
 *     reference counted (allow twice, revoke twice).
 *
 * Allocation is first fit by an in-order walk: O(n) in the number of
 * nodes. drm_mm is O(log n); acceptable for a fallback, see docs/10.
 */
#ifndef _DRMGLUE_VMOFF_H_
#define	_DRMGLUE_VMOFF_H_

#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <drmglue/itree.h>

struct lkpi_vmoff_mgr {
	rwlock_t		lock;
	unsigned long		start;	/* first usable page offset */
	unsigned long		pages;	/* size of the space in pages */
	struct lkpi_itree	tree;
};

struct lkpi_vmoff_tag {
	struct list_head	link;
	const void		*tag;
	unsigned int		count;
};

struct lkpi_vmoff_node {
	struct lkpi_itree_node	it;
	unsigned long		pages;	/* 0 when not allocated */
	rwlock_t		tag_lock;
	struct list_head	tags;
};

void lkpi_vmoff_mgr_init(struct lkpi_vmoff_mgr *m, unsigned long page_offset,
    unsigned long pages);
void lkpi_vmoff_mgr_destroy(struct lkpi_vmoff_mgr *m);

void lkpi_vmoff_node_init(struct lkpi_vmoff_node *n);
/* Frees the remaining tags; the node must be removed first. */
void lkpi_vmoff_node_fini(struct lkpi_vmoff_node *n);

/* 0 (also when already allocated), -EINVAL for pages == 0, -ENOSPC. */
int lkpi_vmoff_add(struct lkpi_vmoff_mgr *m, struct lkpi_vmoff_node *n,
    unsigned long pages);
void lkpi_vmoff_remove(struct lkpi_vmoff_mgr *m, struct lkpi_vmoff_node *n);

static inline void
lkpi_vmoff_lock_lookup(struct lkpi_vmoff_mgr *m)
{
	read_lock(&m->lock);
}

static inline void
lkpi_vmoff_unlock_lookup(struct lkpi_vmoff_mgr *m)
{
	read_unlock(&m->lock);
}

struct lkpi_vmoff_node *lkpi_vmoff_lookup_locked(struct lkpi_vmoff_mgr *m,
    unsigned long start, unsigned long pages);

static inline bool
lkpi_vmoff_node_has_offset(const struct lkpi_vmoff_node *n)
{
	return (n->pages != 0);
}

static inline unsigned long
lkpi_vmoff_node_start(const struct lkpi_vmoff_node *n)
{
	return (n->it.start);
}

/* Byte offset userland passes to mmap(2); 0 when not allocated. */
static inline uint64_t
lkpi_vmoff_node_offset_addr(const struct lkpi_vmoff_node *n, int page_shift)
{
	return (n->pages != 0 ? (uint64_t)n->it.start << page_shift : 0);
}

int lkpi_vmoff_allow(struct lkpi_vmoff_node *n, const void *tag);
void lkpi_vmoff_revoke(struct lkpi_vmoff_node *n, const void *tag);
bool lkpi_vmoff_is_allowed(struct lkpi_vmoff_node *n, const void *tag);

#endif /* _DRMGLUE_VMOFF_H_ */
