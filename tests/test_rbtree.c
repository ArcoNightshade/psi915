/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Unit tests for <linux/rbtree.h> and <linux/rbtree_augmented.h>: random
 * insert/erase sequences checked against the red-black invariants, the
 * cached-leftmost and helper APIs, postorder iteration, replace, and an
 * augmented "subtree maximum" tree.
 */

#include <linux/kernel.h>
#include <linux/rbtree.h>
#include <linux/rbtree_augmented.h>

#include "harness.h"

struct tnode {
	struct rb_node	rb;
	int		key;
	int		val;		/* augmented: max of val in subtree */
	int		subtree_max;
	bool		in_tree;
};

#define	N	512
static struct tnode nodes[N];

static unsigned int rng = 777;

static int
rnd(int mod)
{
	rng = rng * 1103515245U + 12345U;
	return ((int)((rng >> 16) & 0x7fff) % mod);
}

static int
color(const struct rb_node *n)
{
	return (n == NULL ? RB_BLACK : (int)(n->__rb_parent_color & 1));
}

/* Returns black height, or -1 if an invariant is broken below n. */
static int
validate(const struct rb_node *n, const struct rb_node *parent, int lo,
    int hi, int *count)
{
	const struct tnode *t;
	int lh, rh;

	if (n == NULL)
		return (1);
	t = rb_entry(n, struct tnode, rb);
	if (rb_parent(n) != parent)
		return (-1);
	if (t->key < lo || t->key > hi)
		return (-1);
	if (color(n) == RB_RED &&
	    (color(n->rb_left) == RB_RED || color(n->rb_right) == RB_RED))
		return (-1);
	lh = validate(n->rb_left, n, lo, t->key, count);
	rh = validate(n->rb_right, n, t->key, hi, count);
	if (lh < 0 || rh < 0 || lh != rh)
		return (-1);
	(*count)++;
	return (lh + (color(n) == RB_BLACK));
}

static bool
valid_tree(const struct rb_root *root, int expect)
{
	int count = 0;

	if (root->rb_node != NULL && color(root->rb_node) != RB_BLACK)
		return (false);
	if (validate(root->rb_node, NULL, -1, 1 << 30, &count) < 0)
		return (false);
	return (count == expect);
}

static void
insert_plain(struct rb_root *root, struct tnode *t)
{
	struct rb_node **link = &root->rb_node, *parent = NULL;

	while (*link != NULL) {
		parent = *link;
		if (t->key < rb_entry(parent, struct tnode, rb)->key)
			link = &parent->rb_left;
		else
			link = &parent->rb_right;
	}
	rb_link_node(&t->rb, parent, link);
	rb_insert_color(&t->rb, root);
	t->in_tree = true;
}

TEST(random_insert_erase)
{
	struct rb_root root = RB_ROOT;
	struct rb_node *n;
	int i, round, count = 0, prev, seen;

	for (i = 0; i < N; i++) {
		nodes[i].key = rnd(1000);	/* duplicates allowed */
		nodes[i].in_tree = false;
		RB_CLEAR_NODE(&nodes[i].rb);
		CHECK(RB_EMPTY_NODE(&nodes[i].rb));
	}
	CHECK(RB_EMPTY_ROOT(&root));
	CHECK(rb_first(&root) == NULL);
	for (round = 0; round < 4000; round++) {
		struct tnode *t = &nodes[rnd(N)];

		if (t->in_tree) {
			rb_erase(&t->rb, &root);
			RB_CLEAR_NODE(&t->rb);
			t->in_tree = false;
			count--;
		} else {
			insert_plain(&root, t);
			count++;
		}
		if (round % 97 == 0)
			CHECK(valid_tree(&root, count));
	}
	CHECK(valid_tree(&root, count));

	/* In-order walk is sorted, both directions, and visits all. */
	prev = -1;
	seen = 0;
	for (n = rb_first(&root); n != NULL; n = rb_next(n)) {
		CHECK(rb_entry(n, struct tnode, rb)->key >= prev);
		prev = rb_entry(n, struct tnode, rb)->key;
		seen++;
	}
	CHECK_EQ(seen, count);
	prev = 1 << 30;
	seen = 0;
	for (n = rb_last(&root); n != NULL; n = rb_prev(n)) {
		CHECK(rb_entry(n, struct tnode, rb)->key <= prev);
		prev = rb_entry(n, struct tnode, rb)->key;
		seen++;
	}
	CHECK_EQ(seen, count);

	/* Drain completely. */
	while ((n = rb_first(&root)) != NULL) {
		rb_erase(n, &root);
		rb_entry(n, struct tnode, rb)->in_tree = false;
		count--;
	}
	CHECK_EQ(count, 0);
	CHECK(RB_EMPTY_ROOT(&root));
}

TEST(sequential)
{
	struct rb_root root = RB_ROOT;
	int i;

	for (i = 0; i < N; i++) {
		nodes[i].key = i;
		insert_plain(&root, &nodes[i]);
	}
	CHECK(valid_tree(&root, N));
	for (i = 0; i < N; i += 2)
		rb_erase(&nodes[i].rb, &root);
	CHECK(valid_tree(&root, N / 2));
	for (i = N - 1; i > 0; i -= 2)
		rb_erase(&nodes[i].rb, &root);
	CHECK(RB_EMPTY_ROOT(&root));
}

TEST(postorder_and_replace)
{
	struct rb_root root = RB_ROOT;
	struct tnode *pos, *nxt, spare;
	int i, visited = 0;

	for (i = 0; i < 100; i++) {
		nodes[i].key = i;
		insert_plain(&root, &nodes[i]);
	}
	/* Replace node 50 by spare, in place. */
	spare.key = 50;
	rb_replace_node(&nodes[50].rb, &spare.rb, &root);
	CHECK(valid_tree(&root, 100));
	CHECK(rb_next(&nodes[49].rb) == &spare.rb);
	CHECK(rb_prev(&nodes[51].rb) == &spare.rb);

	/* Postorder visits children before parents; safe to "free". */
	rbtree_postorder_for_each_entry_safe(pos, nxt, &root, rb) {
		if (pos->rb.rb_left != NULL)
			CHECK(rb_entry(pos->rb.rb_left, struct tnode, rb)->val
			    == -1);
		if (pos->rb.rb_right != NULL)
			CHECK(rb_entry(pos->rb.rb_right, struct tnode, rb)->val
			    == -1);
		pos->val = -1;
		visited++;
	}
	CHECK_EQ(visited, 100);
	CHECK(rb_next_postorder(NULL) == NULL);
}

static bool
tnode_less(struct rb_node *a, const struct rb_node *b)
{
	return (rb_entry(a, struct tnode, rb)->key <
	    rb_entry(b, struct tnode, rb)->key);
}

static int
tnode_cmp(struct rb_node *a, const struct rb_node *b)
{
	int x = rb_entry(a, struct tnode, rb)->key;
	int y = rb_entry(b, struct tnode, rb)->key;

	return ((x > y) - (x < y));
}

static int
tnode_ccmp(const struct rb_node *a, const struct rb_node *b)
{
	return (tnode_cmp((struct rb_node *)(uintptr_t)a, b));
}

static int
key_cmp(const void *key, const struct rb_node *n)
{
	int k = *(const int *)key;
	int y = rb_entry(n, struct tnode, rb)->key;

	return ((k > y) - (k < y));
}

TEST(cached_and_helpers)
{
	struct rb_root_cached croot = RB_ROOT_CACHED;
	struct rb_root root = RB_ROOT;
	struct rb_node *n;
	struct tnode extra = { .key = 5 };
	int i, k, matches;

	for (i = 0; i < 64; i++) {
		nodes[i].key = 100 - i;		/* decreasing: always leftmost */
		CHECK(rb_add_cached(&nodes[i].rb, &croot, tnode_less) ==
		    &nodes[i].rb);
	}
	CHECK(rb_first_cached(&croot) == &nodes[63].rb);
	CHECK(rb_first_cached(&croot) == rb_first(&croot.rb_root));
	CHECK(rb_erase_cached(&nodes[63].rb, &croot) == &nodes[62].rb);
	CHECK(rb_first_cached(&croot) == &nodes[62].rb);
	CHECK(rb_erase_cached(&nodes[0].rb, &croot) == NULL);
	CHECK(valid_tree(&croot.rb_root, 62));
	nodes[63].key = 0;
	CHECK(rb_find_add_cached(&nodes[63].rb, &croot, tnode_ccmp) == NULL);
	CHECK(rb_first_cached(&croot) == &nodes[63].rb);

	/* rb_add / rb_find_add / rb_find / rb_find_first / rb_for_each. */
	for (i = 64; i < 128; i++) {
		nodes[i].key = i % 8;
		rb_add(&nodes[i].rb, &root, tnode_less);
	}
	CHECK(valid_tree(&root, 64));
	CHECK(rb_find_add(&extra.rb, &root, tnode_cmp) != NULL);
	k = 3;
	n = rb_find(&k, &root, key_cmp);
	CHECK(n != NULL && rb_entry(n, struct tnode, rb)->key == 3);
	n = rb_find_first(&k, &root, key_cmp);
	CHECK(n != NULL && (rb_prev(n) == NULL ||
	    rb_entry(rb_prev(n), struct tnode, rb)->key < 3));
	matches = 0;
	rb_for_each(n, &k, &root, key_cmp)
		matches++;
	CHECK_EQ(matches, 8);
	k = 42;
	CHECK(rb_find(&k, &root, key_cmp) == NULL);
	CHECK(rb_find_first(&k, &root, key_cmp) == NULL);
	extra.key = 42;
	CHECK(rb_find_add(&extra.rb, &root, tnode_cmp) == NULL);
	CHECK(rb_find(&k, &root, key_cmp) == &extra.rb);
}

/* Augmented tree: subtree_max = max(val) over the subtree. */
#define	TNODE_VAL(t)	((t)->val)
RB_DECLARE_CALLBACKS_MAX(static, tnode_aug, struct tnode, rb, int,
    subtree_max, TNODE_VAL);

static void
insert_aug(struct rb_root *root, struct tnode *t)
{
	struct rb_node **link = &root->rb_node, *parent = NULL;
	struct tnode *p;

	t->subtree_max = t->val;
	while (*link != NULL) {
		parent = *link;
		p = rb_entry(parent, struct tnode, rb);
		if (p->subtree_max < t->val)
			p->subtree_max = t->val;	/* update on the way down */
		link = t->key < p->key ? &parent->rb_left : &parent->rb_right;
	}
	rb_link_node(&t->rb, parent, link);
	rb_insert_augmented(&t->rb, root, &tnode_aug);
	t->in_tree = true;
}

/* Returns subtree max, checking every stored value; -2 on mismatch. */
static int
check_aug(const struct rb_node *n)
{
	const struct tnode *t;
	int m, l, r;

	if (n == NULL)
		return (-1);
	t = rb_entry(n, struct tnode, rb);
	l = check_aug(n->rb_left);
	r = check_aug(n->rb_right);
	if (l == -2 || r == -2)
		return (-2);
	m = t->val;
	if (l > m)
		m = l;
	if (r > m)
		m = r;
	return (m == t->subtree_max ? m : -2);
}

TEST(augmented)
{
	struct rb_root root = RB_ROOT;
	int i, round, count = 0;

	for (i = 0; i < N; i++) {
		nodes[i].key = rnd(10000);
		nodes[i].val = rnd(100000);
		nodes[i].in_tree = false;
	}
	for (round = 0; round < 3000; round++) {
		struct tnode *t = &nodes[rnd(N)];

		if (t->in_tree) {
			rb_erase_augmented(&t->rb, &root, &tnode_aug);
			t->in_tree = false;
			count--;
		} else {
			insert_aug(&root, t);
			count++;
		}
		if (round % 50 == 0) {
			CHECK(check_aug(root.rb_node) != -2);
			CHECK(valid_tree(&root, count));
		}
	}
	CHECK(check_aug(root.rb_node) != -2);
	CHECK(valid_tree(&root, count));
}

LKPI_SUITE(rbtree)
{
	RUN(random_insert_erase);
	RUN(sequential);
	RUN(postorder_and_replace);
	RUN(cached_and_helpers);
	RUN(augmented);
}
