/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation of the Linux augmented red-black tree API
 * from its documented behaviour (Documentation/core-api/rbtree.rst,
 * "Augmented rbtrees"). Each node carries a value computed from its
 * subtree; the tree code keeps it up to date through three callbacks:
 *
 *   propagate(node, stop)  recompute from node up to (excluding) stop,
 *                          stopping early once a value is unchanged
 *   copy(old, new)         new takes old's place: copy the value
 *   rotate(old, new)       new replaces old as subtree root after a
 *                          rotation: new gets old's value, old recomputed
 *
 * As in Linux, the caller of rb_insert_augmented() must already have
 * updated the values along the insertion path.
 */
#ifndef _LINUXKPI_LINUX_RBTREE_AUGMENTED_H_
#define	_LINUXKPI_LINUX_RBTREE_AUGMENTED_H_

#include <linuxkpi/platform.h>
#include <linux/compiler.h>
#include <linux/rbtree.h>

struct rb_augment_callbacks {
	void	(*propagate)(struct rb_node *node, struct rb_node *stop);
	void	(*copy)(struct rb_node *old, struct rb_node *new);
	void	(*rotate)(struct rb_node *old, struct rb_node *new);
};

void lkpi_rb_insert_augmented(struct rb_node *node, struct rb_root *root,
    void (*rotate)(struct rb_node *old, struct rb_node *new));
void lkpi_rb_erase_augmented(struct rb_node *node, struct rb_root *root,
    const struct rb_augment_callbacks *augment);

static inline void
rb_insert_augmented(struct rb_node *node, struct rb_root *root,
    const struct rb_augment_callbacks *augment)
{
	lkpi_rb_insert_augmented(node, root, augment->rotate);
}

static inline void
rb_insert_augmented_cached(struct rb_node *node, struct rb_root_cached *root,
    bool newleft, const struct rb_augment_callbacks *augment)
{
	if (newleft)
		root->rb_leftmost = node;
	rb_insert_augmented(node, &root->rb_root, augment);
}

static inline void
rb_erase_augmented(struct rb_node *node, struct rb_root *root,
    const struct rb_augment_callbacks *augment)
{
	lkpi_rb_erase_augmented(node, root, augment);
}

static inline void
rb_erase_augmented_cached(struct rb_node *node, struct rb_root_cached *root,
    const struct rb_augment_callbacks *augment)
{
	if (root->rb_leftmost == node)
		root->rb_leftmost = rb_next(node);
	rb_erase_augmented(node, &root->rb_root, augment);
}

/*
 * Generate the three callbacks. RBCOMPUTE(node, exit) recomputes
 * node->RBAUGMENTED; when exit is true and the value did not change it
 * returns true (and propagation stops).
 */
#define	RB_DECLARE_CALLBACKS(RBSTATIC, RBNAME, RBSTRUCT, RBFIELD,	\
    RBAUGMENTED, RBCOMPUTE)						\
static inline void							\
RBNAME##_propagate(struct rb_node *rb, struct rb_node *stop)		\
{									\
	while (rb != stop) {						\
		RBSTRUCT *node = rb_entry(rb, RBSTRUCT, RBFIELD);	\
		if (RBCOMPUTE(node, true))				\
			break;						\
		rb = rb_parent(&node->RBFIELD);				\
	}								\
}									\
static inline void							\
RBNAME##_copy(struct rb_node *rb_old, struct rb_node *rb_new)		\
{									\
	RBSTRUCT *old = rb_entry(rb_old, RBSTRUCT, RBFIELD);		\
	RBSTRUCT *new = rb_entry(rb_new, RBSTRUCT, RBFIELD);		\
	new->RBAUGMENTED = old->RBAUGMENTED;				\
}									\
static void								\
RBNAME##_rotate(struct rb_node *rb_old, struct rb_node *rb_new)		\
{									\
	RBSTRUCT *old = rb_entry(rb_old, RBSTRUCT, RBFIELD);		\
	RBSTRUCT *new = rb_entry(rb_new, RBSTRUCT, RBFIELD);		\
	new->RBAUGMENTED = old->RBAUGMENTED;				\
	(void) RBCOMPUTE(old, false);					\
}									\
RBSTATIC const struct rb_augment_callbacks RBNAME = {			\
	.propagate = RBNAME##_propagate,				\
	.copy = RBNAME##_copy,						\
	.rotate = RBNAME##_rotate					\
}

/*
 * Callbacks for the common case where the augmented value is the maximum
 * of RBCOMPUTE(node) over the subtree (interval trees, drm_mm holes).
 */
#define	RB_DECLARE_CALLBACKS_MAX(RBSTATIC, RBNAME, RBSTRUCT, RBFIELD,	\
    RBTYPE, RBAUGMENTED, RBCOMPUTE)					\
static inline bool							\
RBNAME##_compute_max(RBSTRUCT *node, bool exit)				\
{									\
	RBSTRUCT *child;						\
	RBTYPE max = RBCOMPUTE(node);					\
									\
	if (node->RBFIELD.rb_left != NULL) {				\
		child = rb_entry(node->RBFIELD.rb_left, RBSTRUCT, RBFIELD); \
		if (child->RBAUGMENTED > max)				\
			max = child->RBAUGMENTED;			\
	}								\
	if (node->RBFIELD.rb_right != NULL) {				\
		child = rb_entry(node->RBFIELD.rb_right, RBSTRUCT, RBFIELD); \
		if (child->RBAUGMENTED > max)				\
			max = child->RBAUGMENTED;			\
	}								\
	if (exit && node->RBAUGMENTED == max)				\
		return (true);						\
	node->RBAUGMENTED = max;					\
	return (false);							\
}									\
RB_DECLARE_CALLBACKS(RBSTATIC, RBNAME, RBSTRUCT, RBFIELD,		\
    RBAUGMENTED, RBNAME##_compute_max)

/* Colour/parent accessors some augmented-tree users need. */
#define	__rb_parent(pc)		((struct rb_node *)((pc) & ~3UL))
#define	__rb_color(pc)		((pc) & 1)
#define	__rb_is_black(pc)	__rb_color(pc)
#define	__rb_is_red(pc)		(!__rb_color(pc))
#define	rb_color(rb)		__rb_color((rb)->__rb_parent_color)
#define	rb_is_red(rb)		__rb_is_red((rb)->__rb_parent_color)
#define	rb_is_black(rb)		__rb_is_black((rb)->__rb_parent_color)

#endif /* _LINUXKPI_LINUX_RBTREE_AUGMENTED_H_ */
