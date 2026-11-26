/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Generic unsigned-long interval tree (src/linux_itree.c-style API).
 */
#ifndef _LINUXKPI_INCLUDE_LINUX_INTERVAL_TREE_H_
#define	_LINUXKPI_INCLUDE_LINUX_INTERVAL_TREE_H_

#include <linux/rbtree.h>

struct interval_tree_node {
	struct rb_node	rb;
	unsigned long	start;
	unsigned long	last;
	unsigned long	__subtree_last;
};

void interval_tree_insert(struct interval_tree_node *node,
    struct rb_root_cached *root);
void interval_tree_remove(struct interval_tree_node *node,
    struct rb_root_cached *root);
struct interval_tree_node *interval_tree_iter_first(struct rb_root_cached *root,
    unsigned long start, unsigned long last);
struct interval_tree_node *interval_tree_iter_next(struct interval_tree_node *node,
    unsigned long start, unsigned long last);

#endif /* _LINUXKPI_INCLUDE_LINUX_INTERVAL_TREE_H_ */
