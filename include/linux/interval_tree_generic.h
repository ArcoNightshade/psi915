/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Interval tree generator INTERVAL_TREE_DEFINE(type, rbfield, keytype,
 * subtree_last, start(node), last(node), static, prefix): closed
 * intervals [start, last] in an augmented rb_root_cached ordered by start,
 * each node caching the largest `last` of its subtree, generating
 * prefix_insert, prefix_remove, prefix_iter_first and prefix_iter_next
 * with the documented Linux semantics (iteration in start order over the
 * intervals overlapping [start, last]).
 */
#ifndef _LINUXKPI_INCLUDE_LINUX_INTERVAL_TREE_GENERIC_H_
#define	_LINUXKPI_INCLUDE_LINUX_INTERVAL_TREE_GENERIC_H_

#include <linux/rbtree_augmented.h>

#define	INTERVAL_TREE_DEFINE(ITSTRUCT, ITRB, ITTYPE, ITSUBTREE,		\
    ITSTART, ITLAST, ITSTATIC, ITPREFIX)				\
									\
RB_DECLARE_CALLBACKS_MAX(static, ITPREFIX ## _augment, ITSTRUCT, ITRB,	\
    ITTYPE, ITSUBTREE, ITLAST);						\
									\
ITSTATIC void								\
ITPREFIX ## _insert(ITSTRUCT *node, struct rb_root_cached *root)	\
{									\
	struct rb_node **link = &root->rb_root.rb_node, *rb_parent = NULL; \
	ITTYPE start = ITSTART(node), last = ITLAST(node);		\
	ITSTRUCT *parent;						\
	bool leftmost = true;						\
									\
	while (*link != NULL) {						\
		rb_parent = *link;					\
		parent = rb_entry(rb_parent, ITSTRUCT, ITRB);		\
		if (parent->ITSUBTREE < last)				\
			parent->ITSUBTREE = last;			\
		if (start < ITSTART(parent)) {				\
			link = &parent->ITRB.rb_left;			\
		} else {						\
			link = &parent->ITRB.rb_right;			\
			leftmost = false;				\
		}							\
	}								\
	node->ITSUBTREE = last;						\
	rb_link_node(&node->ITRB, rb_parent, link);			\
	rb_insert_augmented_cached(&node->ITRB, root, leftmost,		\
	    &ITPREFIX ## _augment);					\
}									\
									\
ITSTATIC void								\
ITPREFIX ## _remove(ITSTRUCT *node, struct rb_root_cached *root)	\
{									\
	rb_erase_augmented_cached(&node->ITRB, root, &ITPREFIX ## _augment); \
}									\
									\
/* Leftmost node in node's subtree overlapping [start, last]. */	\
static ITSTRUCT *							\
ITPREFIX ## _subtree_search(ITSTRUCT *node, ITTYPE start, ITTYPE last)	\
{									\
	for (;;) {							\
		if (node->ITRB.rb_left != NULL) {			\
			ITSTRUCT *left = rb_entry(node->ITRB.rb_left,	\
			    ITSTRUCT, ITRB);				\
			if (start <= left->ITSUBTREE) {			\
				node = left;				\
				continue;				\
			}						\
		}							\
		if (ITSTART(node) <= last) {				\
			if (start <= ITLAST(node))			\
				return (node);				\
			if (node->ITRB.rb_right != NULL) {		\
				node = rb_entry(node->ITRB.rb_right,	\
				    ITSTRUCT, ITRB);			\
				if (start <= node->ITSUBTREE)		\
					continue;			\
			}						\
		}							\
		return (NULL);						\
	}								\
}									\
									\
ITSTATIC ITSTRUCT *							\
ITPREFIX ## _iter_first(struct rb_root_cached *root, ITTYPE start,	\
    ITTYPE last)							\
{									\
	ITSTRUCT *node, *leftmost;					\
									\
	if (root->rb_root.rb_node == NULL)				\
		return (NULL);						\
	node = rb_entry(root->rb_root.rb_node, ITSTRUCT, ITRB);	\
	if (node->ITSUBTREE < start)					\
		return (NULL);						\
	leftmost = rb_entry(root->rb_leftmost, ITSTRUCT, ITRB);	\
	if (ITSTART(leftmost) > last)					\
		return (NULL);						\
	return (ITPREFIX ## _subtree_search(node, start, last));	\
}									\
									\
ITSTATIC ITSTRUCT *							\
ITPREFIX ## _iter_next(ITSTRUCT *node, ITTYPE start, ITTYPE last)	\
{									\
	struct rb_node *rb = node->ITRB.rb_right, *prev;		\
									\
	for (;;) {							\
		if (rb != NULL) {					\
			ITSTRUCT *right = rb_entry(rb, ITSTRUCT, ITRB); \
			if (start <= right->ITSUBTREE)			\
				return (ITPREFIX ## _subtree_search(right, \
				    start, last));			\
		}							\
		/* Climb until we come up from a left child. */	\
		do {							\
			rb = rb_parent(&node->ITRB);			\
			if (rb == NULL)					\
				return (NULL);				\
			prev = &node->ITRB;				\
			node = rb_entry(rb, ITSTRUCT, ITRB);		\
			rb = node->ITRB.rb_right;			\
		} while (prev == rb);					\
		if (last < ITSTART(node))				\
			return (NULL);					\
		if (start <= ITLAST(node))				\
			return (node);					\
	}								\
}

#endif /* _LINUXKPI_INCLUDE_LINUX_INTERVAL_TREE_GENERIC_H_ */
