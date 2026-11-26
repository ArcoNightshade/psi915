/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * Closed-interval tree over unsigned long keys (page offsets), built on the
 * KPI augmented rbtree: each node caches the largest interval end in its
 * subtree, so "all intervals overlapping [a, b]" costs O(log n + k).
 * Intervals may overlap each other (the address_space mapping registry
 * needs that); the offset manager (vmoff.h) keeps them disjoint itself.
 *
 * Not internally locked. Pure logic; tested in drmglue/tests/test_vmoff.c. Moved from drmglue
 * into the KPI (docs/10 R1) because struct address_space embeds it.
 */
#ifndef _LINUXKPI_ITREE_H_
#define	_LINUXKPI_ITREE_H_

#include <linux/types.h>
#include <linux/rbtree.h>

struct lkpi_itree_node {
	struct rb_node	rb;
	unsigned long	start;		/* first key, inclusive */
	unsigned long	last;		/* last key, inclusive */
	unsigned long	subtree_last;	/* max(last) over the subtree */
};

struct lkpi_itree {
	struct rb_root	root;
	unsigned long	count;
};

#define	LKPI_ITREE_INIT	{ .root = RB_ROOT, .count = 0 }

static inline void
lkpi_itree_init(struct lkpi_itree *t)
{
	t->root = RB_ROOT;
	t->count = 0;
}

static inline bool
lkpi_itree_empty(const struct lkpi_itree *t)
{
	return (t->root.rb_node == NULL);
}

/* Insert n (start <= last must hold); equal starts are allowed. */
void lkpi_itree_insert(struct lkpi_itree *t, struct lkpi_itree_node *n);
void lkpi_itree_remove(struct lkpi_itree *t, struct lkpi_itree_node *n);

/*
 * Overlap iteration in ascending start order:
 *   for (n = lkpi_itree_first(t, a, b); n; n = lkpi_itree_next(n, a, b))
 * The tree must not change during the walk.
 */
struct lkpi_itree_node *lkpi_itree_first(const struct lkpi_itree *t,
    unsigned long a, unsigned long b);
struct lkpi_itree_node *lkpi_itree_next(struct lkpi_itree_node *n,
    unsigned long a, unsigned long b);

/* In-order neighbours regardless of overlap. */
struct lkpi_itree_node *lkpi_itree_min(const struct lkpi_itree *t);
struct lkpi_itree_node *lkpi_itree_succ(struct lkpi_itree_node *n);

#endif /* _LINUXKPI_ITREE_H_ */
