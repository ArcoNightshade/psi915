/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * Red-black tree rebalancing for <linux/rbtree.h> and
 * <linux/rbtree_augmented.h>. Clean-room implementation of the textbook
 * algorithm (Cormen, Leiserson, Rivest, Stein, "Introduction to
 * Algorithms", chapter 13), with NULL leaves, parent pointers packed with
 * the colour, and hooks for augmented values.
 *
 * Invariants: the root is black; a red node has no red child; every path
 * from a node to a NULL leaf has the same number of black nodes.
 */

#include <linux/kernel.h>
#include <linux/rbtree.h>
#include <linux/rbtree_augmented.h>

static inline int
lkpi_rb_color(const struct rb_node *n)
{
	return ((int)(n->__rb_parent_color & 1));
}

/* NULL leaves count as black. */
static inline bool
lkpi_rb_red(const struct rb_node *n)
{
	return (n != NULL && lkpi_rb_color(n) == RB_RED);
}

static inline void
lkpi_rb_set_parent(struct rb_node *n, struct rb_node *p)
{
	n->__rb_parent_color = (n->__rb_parent_color & 1) | (unsigned long)p;
}

static inline void
lkpi_rb_set_color(struct rb_node *n, int color)
{
	n->__rb_parent_color = (n->__rb_parent_color & ~1UL) |
	    (unsigned long)color;
}

/* Make parent (or the root) point to new instead of old. */
static inline void
lkpi_rb_change_child(struct rb_node *old, struct rb_node *new,
    struct rb_node *parent, struct rb_root *root)
{
	if (parent == NULL)
		WRITE_ONCE(root->rb_node, new);
	else if (parent->rb_left == old)
		WRITE_ONCE(parent->rb_left, new);
	else
		WRITE_ONCE(parent->rb_right, new);
}

static void
lkpi_rb_noop_rotate(struct rb_node *old, struct rb_node *new)
{
	(void) old;
	(void) new;
}

static void
lkpi_rb_noop_propagate(struct rb_node *node, struct rb_node *stop)
{
	(void) node;
	(void) stop;
}

static const struct rb_augment_callbacks lkpi_rb_noop_callbacks = {
	.propagate = lkpi_rb_noop_propagate,
	.copy = lkpi_rb_noop_rotate,
	.rotate = lkpi_rb_noop_rotate,
};

/*
 *     x                y
 *    / \              / \
 *   a   y     =>     x   c
 *      / \          / \
 *     b   c        a   b
 */
static void
lkpi_rb_rotate_left(struct rb_node *x, struct rb_root *root,
    void (*rotate)(struct rb_node *, struct rb_node *))
{
	struct rb_node *y = x->rb_right;
	struct rb_node *p = rb_parent(x);

	x->rb_right = y->rb_left;
	if (y->rb_left != NULL)
		lkpi_rb_set_parent(y->rb_left, x);
	lkpi_rb_set_parent(y, p);
	lkpi_rb_change_child(x, y, p, root);
	y->rb_left = x;
	lkpi_rb_set_parent(x, y);
	rotate(x, y);
}

/* Mirror image of lkpi_rb_rotate_left. */
static void
lkpi_rb_rotate_right(struct rb_node *x, struct rb_root *root,
    void (*rotate)(struct rb_node *, struct rb_node *))
{
	struct rb_node *y = x->rb_left;
	struct rb_node *p = rb_parent(x);

	x->rb_left = y->rb_right;
	if (y->rb_right != NULL)
		lkpi_rb_set_parent(y->rb_right, x);
	lkpi_rb_set_parent(y, p);
	lkpi_rb_change_child(x, y, p, root);
	y->rb_right = x;
	lkpi_rb_set_parent(x, y);
	rotate(x, y);
}

/* Restore the invariants after node was linked in as a red leaf. */
void
lkpi_rb_insert_augmented(struct rb_node *node, struct rb_root *root,
    void (*rotate)(struct rb_node *, struct rb_node *))
{
	struct rb_node *parent, *gparent, *uncle;

	for (;;) {
		parent = rb_parent(node);
		if (parent == NULL) {
			lkpi_rb_set_color(node, RB_BLACK);	/* root */
			return;
		}
		if (!lkpi_rb_red(parent))
			return;
		/* A red parent is not the root, so gparent exists. */
		gparent = rb_parent(parent);
		if (parent == gparent->rb_left) {
			uncle = gparent->rb_right;
			if (lkpi_rb_red(uncle)) {
				/* Recolour and continue from gparent. */
				lkpi_rb_set_color(parent, RB_BLACK);
				lkpi_rb_set_color(uncle, RB_BLACK);
				lkpi_rb_set_color(gparent, RB_RED);
				node = gparent;
				continue;
			}
			if (node == parent->rb_right) {
				lkpi_rb_rotate_left(parent, root, rotate);
				node = parent;
				parent = rb_parent(node);
			}
			lkpi_rb_set_color(parent, RB_BLACK);
			lkpi_rb_set_color(gparent, RB_RED);
			lkpi_rb_rotate_right(gparent, root, rotate);
			return;
		} else {
			uncle = gparent->rb_left;
			if (lkpi_rb_red(uncle)) {
				lkpi_rb_set_color(parent, RB_BLACK);
				lkpi_rb_set_color(uncle, RB_BLACK);
				lkpi_rb_set_color(gparent, RB_RED);
				node = gparent;
				continue;
			}
			if (node == parent->rb_left) {
				lkpi_rb_rotate_right(parent, root, rotate);
				node = parent;
				parent = rb_parent(node);
			}
			lkpi_rb_set_color(parent, RB_BLACK);
			lkpi_rb_set_color(gparent, RB_RED);
			lkpi_rb_rotate_left(gparent, root, rotate);
			return;
		}
	}
}

void
rb_insert_color(struct rb_node *node, struct rb_root *root)
{
	lkpi_rb_insert_augmented(node, root, lkpi_rb_noop_rotate);
}

/*
 * A black node was removed; x (possibly NULL) now carries an extra
 * "blackness" and sits below parent. Push it up or resolve it.
 */
static void
lkpi_rb_erase_fixup(struct rb_node *x, struct rb_node *parent,
    struct rb_root *root, void (*rotate)(struct rb_node *, struct rb_node *))
{
	struct rb_node *w;

	while (x != root->rb_node && !lkpi_rb_red(x)) {
		/*
		 * The sibling w is never NULL: x's side lost a black node,
		 * so the other side has black height >= 1.
		 */
		if (x == parent->rb_left) {
			w = parent->rb_right;
			if (lkpi_rb_red(w)) {
				lkpi_rb_set_color(w, RB_BLACK);
				lkpi_rb_set_color(parent, RB_RED);
				lkpi_rb_rotate_left(parent, root, rotate);
				w = parent->rb_right;
			}
			if (!lkpi_rb_red(w->rb_left) &&
			    !lkpi_rb_red(w->rb_right)) {
				lkpi_rb_set_color(w, RB_RED);
				x = parent;
				parent = rb_parent(x);
				continue;
			}
			if (!lkpi_rb_red(w->rb_right)) {
				lkpi_rb_set_color(w->rb_left, RB_BLACK);
				lkpi_rb_set_color(w, RB_RED);
				lkpi_rb_rotate_right(w, root, rotate);
				w = parent->rb_right;
			}
			lkpi_rb_set_color(w, lkpi_rb_color(parent));
			lkpi_rb_set_color(parent, RB_BLACK);
			lkpi_rb_set_color(w->rb_right, RB_BLACK);
			lkpi_rb_rotate_left(parent, root, rotate);
			x = root->rb_node;
			break;
		} else {
			w = parent->rb_left;
			if (lkpi_rb_red(w)) {
				lkpi_rb_set_color(w, RB_BLACK);
				lkpi_rb_set_color(parent, RB_RED);
				lkpi_rb_rotate_right(parent, root, rotate);
				w = parent->rb_left;
			}
			if (!lkpi_rb_red(w->rb_left) &&
			    !lkpi_rb_red(w->rb_right)) {
				lkpi_rb_set_color(w, RB_RED);
				x = parent;
				parent = rb_parent(x);
				continue;
			}
			if (!lkpi_rb_red(w->rb_left)) {
				lkpi_rb_set_color(w->rb_right, RB_BLACK);
				lkpi_rb_set_color(w, RB_RED);
				lkpi_rb_rotate_left(w, root, rotate);
				w = parent->rb_left;
			}
			lkpi_rb_set_color(w, lkpi_rb_color(parent));
			lkpi_rb_set_color(parent, RB_BLACK);
			lkpi_rb_set_color(w->rb_left, RB_BLACK);
			lkpi_rb_rotate_right(parent, root, rotate);
			x = root->rb_node;
			break;
		}
	}
	if (x != NULL)
		lkpi_rb_set_color(x, RB_BLACK);
}

void
lkpi_rb_erase_augmented(struct rb_node *z, struct rb_root *root,
    const struct rb_augment_callbacks *aug)
{
	struct rb_node *child, *parent, *y;
	int color;

	if (z->rb_left == NULL || z->rb_right == NULL) {
		/* At most one child: splice z out. */
		child = z->rb_left != NULL ? z->rb_left : z->rb_right;
		parent = rb_parent(z);
		color = lkpi_rb_color(z);
		lkpi_rb_change_child(z, child, parent, root);
		if (child != NULL)
			lkpi_rb_set_parent(child, parent);
		aug->propagate(parent, NULL);
	} else {
		/* Two children: the successor y takes z's place. */
		y = z->rb_right;
		while (y->rb_left != NULL)
			y = y->rb_left;
		color = lkpi_rb_color(y);
		child = y->rb_right;
		if (rb_parent(y) == z) {
			parent = y;
		} else {
			parent = rb_parent(y);
			parent->rb_left = child;
			if (child != NULL)
				lkpi_rb_set_parent(child, parent);
			y->rb_right = z->rb_right;
			lkpi_rb_set_parent(z->rb_right, y);
		}
		y->rb_left = z->rb_left;
		lkpi_rb_set_parent(z->rb_left, y);
		lkpi_rb_change_child(z, y, rb_parent(z), root);
		y->__rb_parent_color = z->__rb_parent_color;
		aug->copy(z, y);
		aug->propagate(parent, y);
		aug->propagate(y, NULL);
	}
	if (color == RB_BLACK)
		lkpi_rb_erase_fixup(child, parent, root, aug->rotate);
}

void
rb_erase(struct rb_node *node, struct rb_root *root)
{
	lkpi_rb_erase_augmented(node, root, &lkpi_rb_noop_callbacks);
}

struct rb_node *
rb_first(const struct rb_root *root)
{
	struct rb_node *n = root->rb_node;

	if (n == NULL)
		return (NULL);
	while (n->rb_left != NULL)
		n = n->rb_left;
	return (n);
}

struct rb_node *
rb_last(const struct rb_root *root)
{
	struct rb_node *n = root->rb_node;

	if (n == NULL)
		return (NULL);
	while (n->rb_right != NULL)
		n = n->rb_right;
	return (n);
}

struct rb_node *
rb_next(const struct rb_node *node)
{
	struct rb_node *parent;

	if (RB_EMPTY_NODE(node))
		return (NULL);
	if (node->rb_right != NULL) {
		node = node->rb_right;
		while (node->rb_left != NULL)
			node = node->rb_left;
		return ((struct rb_node *)(uintptr_t)node);
	}
	while ((parent = rb_parent(node)) != NULL && node == parent->rb_right)
		node = parent;
	return (parent);
}

struct rb_node *
rb_prev(const struct rb_node *node)
{
	struct rb_node *parent;

	if (RB_EMPTY_NODE(node))
		return (NULL);
	if (node->rb_left != NULL) {
		node = node->rb_left;
		while (node->rb_right != NULL)
			node = node->rb_right;
		return ((struct rb_node *)(uintptr_t)node);
	}
	while ((parent = rb_parent(node)) != NULL && node == parent->rb_left)
		node = parent;
	return (parent);
}

/* Deepest node reached by preferring left, then right, children. */
static struct rb_node *
lkpi_rb_left_deepest(const struct rb_node *node)
{
	for (;;) {
		if (node->rb_left != NULL)
			node = node->rb_left;
		else if (node->rb_right != NULL)
			node = node->rb_right;
		else
			return ((struct rb_node *)(uintptr_t)node);
	}
}

struct rb_node *
rb_first_postorder(const struct rb_root *root)
{
	if (root->rb_node == NULL)
		return (NULL);
	return (lkpi_rb_left_deepest(root->rb_node));
}

/* Children before parents, so nodes can be freed while iterating. */
struct rb_node *
rb_next_postorder(const struct rb_node *node)
{
	struct rb_node *parent;

	if (node == NULL)
		return (NULL);
	parent = rb_parent(node);
	if (parent != NULL && node == parent->rb_left &&
	    parent->rb_right != NULL)
		return (lkpi_rb_left_deepest(parent->rb_right));
	return (parent);
}

void
rb_replace_node(struct rb_node *victim, struct rb_node *new,
    struct rb_root *root)
{
	struct rb_node *parent = rb_parent(victim);

	*new = *victim;
	if (victim->rb_left != NULL)
		lkpi_rb_set_parent(victim->rb_left, new);
	if (victim->rb_right != NULL)
		lkpi_rb_set_parent(victim->rb_right, new);
	lkpi_rb_change_child(victim, new, parent, root);
}
