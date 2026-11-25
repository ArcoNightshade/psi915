/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * Augmented-rbtree interval tree (textbook construction: each node keeps
 * the maximum interval end of its subtree; Cormen et al., "Introduction to
 * Algorithms", section 14.3). See <drmglue/itree.h>.
 */

#include <linux/kernel.h>
#include <linux/rbtree.h>
#include <linux/rbtree_augmented.h>
#include <drmglue/itree.h>

#define	ITN(rbp)	rb_entry((rbp), struct lkpi_itree_node, rb)
#define	ITLAST(n)	((n)->last)

RB_DECLARE_CALLBACKS_MAX(static, lkpi_itree_cb, struct lkpi_itree_node, rb,
    unsigned long, subtree_last, ITLAST);

void
lkpi_itree_insert(struct lkpi_itree *t, struct lkpi_itree_node *n)
{
	struct rb_node **link = &t->root.rb_node, *parent = NULL;
	struct lkpi_itree_node *p;

	while (*link != NULL) {
		parent = *link;
		p = ITN(parent);
		if (p->subtree_last < n->last)
			p->subtree_last = n->last;
		if (n->start < p->start)
			link = &parent->rb_left;
		else
			link = &parent->rb_right;
	}
	n->subtree_last = n->last;
	rb_link_node(&n->rb, parent, link);
	rb_insert_augmented(&n->rb, &t->root, &lkpi_itree_cb);
	t->count++;
}

void
lkpi_itree_remove(struct lkpi_itree *t, struct lkpi_itree_node *n)
{
	rb_erase_augmented(&n->rb, &t->root, &lkpi_itree_cb);
	t->count--;
}

/*
 * Leftmost node of the subtree rooted at n that overlaps [a, b], or NULL.
 * Precondition: n->subtree_last >= a.
 */
static struct lkpi_itree_node *
lkpi_itree_subtree_search(struct lkpi_itree_node *n, unsigned long a,
    unsigned long b)
{
	struct lkpi_itree_node *l, *r;

	for (;;) {
		if (n->rb.rb_left != NULL) {
			l = ITN(n->rb.rb_left);
			if (l->subtree_last >= a) {
				/*
				 * Some interval on the left ends at or after
				 * a and starts no later than n. If n starts
				 * within [.., b] that interval overlaps and
				 * precedes n; if not, nothing at or after n
				 * can overlap. Either way, go left.
				 */
				n = l;
				continue;
			}
		}
		if (n->start > b)
			return (NULL);
		if (n->last >= a)
			return (n);
		if (n->rb.rb_right == NULL)
			return (NULL);
		r = ITN(n->rb.rb_right);
		if (r->subtree_last < a)
			return (NULL);
		n = r;
	}
}

struct lkpi_itree_node *
lkpi_itree_first(const struct lkpi_itree *t, unsigned long a, unsigned long b)
{
	struct lkpi_itree_node *root;

	if (t->root.rb_node == NULL || a > b)
		return (NULL);
	root = ITN(t->root.rb_node);
	if (root->subtree_last < a)
		return (NULL);
	return (lkpi_itree_subtree_search(root, a, b));
}

struct lkpi_itree_node *
lkpi_itree_next(struct lkpi_itree_node *n, unsigned long a, unsigned long b)
{
	struct rb_node *rb = n->rb.rb_right, *prev;

	for (;;) {
		if (rb != NULL && ITN(rb)->subtree_last >= a)
			return (lkpi_itree_subtree_search(ITN(rb), a, b));
		/* Climb until we arrive from a left child. */
		do {
			prev = &n->rb;
			rb = rb_parent(&n->rb);
			if (rb == NULL)
				return (NULL);
			n = ITN(rb);
			rb = n->rb.rb_right;
		} while (prev == rb);
		/* n is the in-order successor of the previous node. */
		if (n->start > b)
			return (NULL);
		if (n->last >= a)
			return (n);
	}
}

struct lkpi_itree_node *
lkpi_itree_min(const struct lkpi_itree *t)
{
	struct rb_node *rb = rb_first(&t->root);

	return (rb != NULL ? ITN(rb) : NULL);
}

struct lkpi_itree_node *
lkpi_itree_succ(struct lkpi_itree_node *n)
{
	struct rb_node *rb = rb_next(&n->rb);

	return (rb != NULL ? ITN(rb) : NULL);
}
