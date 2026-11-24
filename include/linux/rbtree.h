/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation of the Linux red-black tree API, from its
 * documented behaviour (Documentation/core-api/rbtree.rst) and the
 * textbook algorithm (Cormen et al.). Node layout follows Linux: the parent
 * pointer and the colour share one word (colour in bit 0, red = 0), so
 * users can embed struct rb_node and do their own search/link, then call
 * rb_insert_color(). Rebalancing is in src/linux_rbtree.c; augmented trees
 * are in <linux/rbtree_augmented.h>.
 *
 * Not provided yet: the _rcu variants (lockless lookups during updates).
 */
#ifndef _LINUXKPI_LINUX_RBTREE_H_
#define	_LINUXKPI_LINUX_RBTREE_H_

#include <linuxkpi/platform.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/container_of.h>
#include <linux/stddef.h>

struct rb_node {
	unsigned long	__rb_parent_color;
	struct rb_node	*rb_right;
	struct rb_node	*rb_left;
} __attribute__((__aligned__(sizeof (long))));

struct rb_root {
	struct rb_node	*rb_node;
};

/* Tree with a cached pointer to its leftmost (smallest) node. */
struct rb_root_cached {
	struct rb_root	rb_root;
	struct rb_node	*rb_leftmost;
};

#define	RB_RED		0
#define	RB_BLACK	1

#define	rb_parent(r)	((struct rb_node *)((r)->__rb_parent_color & ~3UL))

#define	RB_ROOT			((struct rb_root) { NULL })
#define	RB_ROOT_CACHED		((struct rb_root_cached) { { NULL }, NULL })

#define	rb_entry(ptr, type, member)	container_of(ptr, type, member)
#define	rb_entry_safe(ptr, type, member) ({				\
	typeof(ptr) __lkpi_rbp = (ptr);					\
	__lkpi_rbp != NULL ? rb_entry(__lkpi_rbp, type, member) : NULL;	\
})

#define	RB_EMPTY_ROOT(root)	(READ_ONCE((root)->rb_node) == NULL)
/* A node that is not in any tree points to itself (after RB_CLEAR_NODE). */
#define	RB_EMPTY_NODE(node)						\
	((node)->__rb_parent_color == (unsigned long)(node))
#define	RB_CLEAR_NODE(node)						\
	((node)->__rb_parent_color = (unsigned long)(node))

void rb_insert_color(struct rb_node *node, struct rb_root *root);
void rb_erase(struct rb_node *node, struct rb_root *root);
struct rb_node *rb_next(const struct rb_node *node);
struct rb_node *rb_prev(const struct rb_node *node);
struct rb_node *rb_first(const struct rb_root *root);
struct rb_node *rb_last(const struct rb_root *root);
struct rb_node *rb_first_postorder(const struct rb_root *root);
struct rb_node *rb_next_postorder(const struct rb_node *node);
void rb_replace_node(struct rb_node *victim, struct rb_node *new,
    struct rb_root *root);

/* Attach node as a red leaf at *link below parent; then rb_insert_color. */
static inline void
rb_link_node(struct rb_node *node, struct rb_node *parent,
    struct rb_node **link)
{
	node->__rb_parent_color = (unsigned long)parent;
	node->rb_left = node->rb_right = NULL;
	*link = node;
}

#define	rbtree_postorder_for_each_entry_safe(pos, n, root, field)	\
	for ((pos) = rb_entry_safe(rb_first_postorder(root),		\
	    typeof(*(pos)), field);					\
	    (pos) != NULL && ({ (n) = rb_entry_safe(			\
	    rb_next_postorder(&(pos)->field), typeof(*(pos)), field); 1; }); \
	    (pos) = (n))

/* Cached-leftmost variants. */
#define	rb_first_cached(root)	((root)->rb_leftmost)

static inline void
rb_insert_color_cached(struct rb_node *node, struct rb_root_cached *root,
    bool leftmost)
{
	if (leftmost)
		root->rb_leftmost = node;
	rb_insert_color(node, &root->rb_root);
}

/* Returns the new leftmost node if node was the leftmost, else NULL. */
static inline struct rb_node *
rb_erase_cached(struct rb_node *node, struct rb_root_cached *root)
{
	struct rb_node *leftmost = NULL;

	if (root->rb_leftmost == node)
		leftmost = root->rb_leftmost = rb_next(node);
	rb_erase(node, &root->rb_root);
	return (leftmost);
}

static inline void
rb_replace_node_cached(struct rb_node *victim, struct rb_node *new,
    struct rb_root_cached *root)
{
	if (root->rb_leftmost == victim)
		root->rb_leftmost = new;
	rb_replace_node(victim, new, &root->rb_root);
}

/*
 * Helpers that do the search themselves. less(a, b): a sorts before b.
 * cmp(key, node): < 0, 0, > 0 as key sorts before, at, after node.
 */
static inline struct rb_node *
rb_add_cached(struct rb_node *node, struct rb_root_cached *tree,
    bool (*less)(struct rb_node *, const struct rb_node *))
{
	struct rb_node **link = &tree->rb_root.rb_node;
	struct rb_node *parent = NULL;
	bool leftmost = true;

	while (*link != NULL) {
		parent = *link;
		if (less(node, parent)) {
			link = &parent->rb_left;
		} else {
			link = &parent->rb_right;
			leftmost = false;
		}
	}
	rb_link_node(node, parent, link);
	rb_insert_color_cached(node, tree, leftmost);
	return (leftmost ? node : NULL);
}

static inline void
rb_add(struct rb_node *node, struct rb_root *tree,
    bool (*less)(struct rb_node *, const struct rb_node *))
{
	struct rb_node **link = &tree->rb_node;
	struct rb_node *parent = NULL;

	while (*link != NULL) {
		parent = *link;
		link = less(node, parent) ? &parent->rb_left : &parent->rb_right;
	}
	rb_link_node(node, parent, link);
	rb_insert_color(node, tree);
}

/* Insert node unless an equal one exists; returns that one, or NULL. */
static inline struct rb_node *
rb_find_add(struct rb_node *node, struct rb_root *tree,
    int (*cmp)(struct rb_node *, const struct rb_node *))
{
	struct rb_node **link = &tree->rb_node;
	struct rb_node *parent = NULL;
	int c;

	while (*link != NULL) {
		parent = *link;
		c = cmp(node, parent);
		if (c < 0)
			link = &parent->rb_left;
		else if (c > 0)
			link = &parent->rb_right;
		else
			return (parent);
	}
	rb_link_node(node, parent, link);
	rb_insert_color(node, tree);
	return (NULL);
}

static inline struct rb_node *
rb_find_add_cached(struct rb_node *node, struct rb_root_cached *tree,
    int (*cmp)(const struct rb_node *, const struct rb_node *))
{
	struct rb_node **link = &tree->rb_root.rb_node;
	struct rb_node *parent = NULL;
	bool leftmost = true;
	int c;

	while (*link != NULL) {
		parent = *link;
		c = cmp(node, parent);
		if (c < 0) {
			link = &parent->rb_left;
		} else if (c > 0) {
			link = &parent->rb_right;
			leftmost = false;
		} else {
			return (parent);
		}
	}
	rb_link_node(node, parent, link);
	rb_insert_color_cached(node, tree, leftmost);
	return (NULL);
}

static inline struct rb_node *
rb_find(const void *key, const struct rb_root *tree,
    int (*cmp)(const void *key, const struct rb_node *))
{
	struct rb_node *node = tree->rb_node;
	int c;

	while (node != NULL) {
		c = cmp(key, node);
		if (c < 0)
			node = node->rb_left;
		else if (c > 0)
			node = node->rb_right;
		else
			return (node);
	}
	return (NULL);
}

/* Leftmost node equal to key, or NULL. */
static inline struct rb_node *
rb_find_first(const void *key, const struct rb_root *tree,
    int (*cmp)(const void *key, const struct rb_node *))
{
	struct rb_node *node = tree->rb_node, *match = NULL;
	int c;

	while (node != NULL) {
		c = cmp(key, node);
		if (c <= 0) {
			if (c == 0)
				match = node;
			node = node->rb_left;
		} else {
			node = node->rb_right;
		}
	}
	return (match);
}

/* Next node after node if it is also equal to key, else NULL. */
static inline struct rb_node *
rb_next_match(const void *key, struct rb_node *node,
    int (*cmp)(const void *key, const struct rb_node *))
{
	node = rb_next(node);
	if (node != NULL && cmp(key, node) != 0)
		node = NULL;
	return (node);
}

#define	rb_for_each(node, key, tree, cmp)				\
	for ((node) = rb_find_first((key), (tree), (cmp));		\
	    (node) != NULL; (node) = rb_next_match((key), (node), (cmp)))

#endif /* _LINUXKPI_LINUX_RBTREE_H_ */
