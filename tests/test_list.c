/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Unit tests for <linux/list.h> (list_head and hlist).
 */

#include <linux/list.h>
#include <linux/kernel.h>

#include "harness.h"

struct item {
	int		val;
	struct list_head node;
	struct hlist_node hnode;
};

#define	NITEMS	8
static struct item items[NITEMS];

static void
init_items(void)
{
	int i;

	for (i = 0; i < NITEMS; i++) {
		items[i].val = i;
		INIT_LIST_HEAD(&items[i].node);
		INIT_HLIST_NODE(&items[i].hnode);
	}
}

/* Fill head with items[0..n-1] in order. */
static void
fill(struct list_head *head, int n)
{
	int i;

	INIT_LIST_HEAD(head);
	for (i = 0; i < n; i++)
		list_add_tail(&items[i].node, head);
}

/* Check that the list holds exactly the given values, in both directions. */
static int
list_matches(struct list_head *head, const int *vals, int n)
{
	struct item *it;
	int i = 0;

	list_for_each_entry(it, head, node) {
		if (i >= n || it->val != vals[i])
			return (0);
		i++;
	}
	if (i != n)
		return (0);
	list_for_each_entry_reverse(it, head, node) {
		if (--i < 0 || it->val != vals[i])
			return (0);
	}
	return (i == 0 && list_count_nodes(head) == (size_t)n);
}

TEST(container_of)
{
	struct item it;

	CHECK(container_of(&it.node, struct item, node) == &it);
	CHECK(list_entry(&it.hnode, struct item, hnode) == &it);
	CHECK(offsetofend(struct item, val) == sizeof(int));
}

TEST(static_head)
{
	LIST_HEAD(head);
	HLIST_HEAD(hhead);

	CHECK(list_empty(&head));
	CHECK(list_empty_careful(&head));
	CHECK(head.next == &head && head.prev == &head);
	CHECK(hlist_empty(&hhead));
}

TEST(add_and_add_tail)
{
	LIST_HEAD(head);
	static const int want[] = { 2, 1, 0, 3, 4 };

	init_items();
	list_add(&items[0].node, &head);
	list_add(&items[1].node, &head);
	list_add(&items[2].node, &head);
	list_add_tail(&items[3].node, &head);
	list_add_tail(&items[4].node, &head);
	CHECK(list_matches(&head, want, 5));
	CHECK(list_first_entry(&head, struct item, node)->val == 2);
	CHECK(list_last_entry(&head, struct item, node)->val == 4);
	CHECK(list_is_first(&items[2].node, &head));
	CHECK(list_is_last(&items[4].node, &head));
	CHECK(!list_is_singular(&head));
	CHECK(list_is_head(&head, &head));
}

TEST(del)
{
	struct list_head head;
	static const int want[] = { 0, 2, 4 };

	init_items();
	fill(&head, 5);
	list_del(&items[1].node);
	list_del_init(&items[3].node);
	CHECK(list_empty(&items[3].node));
	CHECK(list_matches(&head, want, 3));

	list_del(&items[0].node);
	list_del(&items[2].node);
	CHECK(list_is_singular(&head));
	list_del_init(&items[4].node);
	CHECK(list_empty(&head));
	CHECK(list_first_entry_or_null(&head, struct item, node) == NULL);
}

TEST(replace_and_swap)
{
	struct list_head head;
	static const int want1[] = { 0, 5, 2 };
	static const int want2[] = { 2, 5, 0 };
	static const int want3[] = { 5, 2, 0 };

	init_items();
	fill(&head, 3);
	list_replace_init(&items[1].node, &items[5].node);
	CHECK(list_empty(&items[1].node));
	CHECK(list_matches(&head, want1, 3));

	list_swap(&items[0].node, &items[2].node);
	CHECK(list_matches(&head, want2, 3));
	/* Adjacent entries. */
	list_swap(&items[2].node, &items[5].node);
	CHECK(list_matches(&head, want3, 3));
}

TEST(move_and_rotate)
{
	struct list_head head, other;
	static const int want1[] = { 3, 0, 1, 2, 4 };
	static const int want2[] = { 0, 1, 2, 4, 3 };
	static const int want3[] = { 1, 2, 4, 3, 0 };
	static const int want4[] = { 4, 3, 0, 1, 2 };

	init_items();
	fill(&head, 5);
	list_move(&items[3].node, &head);
	CHECK(list_matches(&head, want1, 5));
	list_move_tail(&items[3].node, &head);
	CHECK(list_matches(&head, want2, 5));
	list_rotate_left(&head);
	CHECK(list_matches(&head, want3, 5));
	list_rotate_to_front(&items[4].node, &head);
	CHECK(list_matches(&head, want4, 5));

	INIT_LIST_HEAD(&other);
	list_move(&items[0].node, &other);
	CHECK(list_is_singular(&other));
	CHECK(list_count_nodes(&head) == 4);
}

TEST(bulk_move_tail)
{
	struct list_head head;
	static const int want[] = { 0, 4, 5, 1, 2, 3 };

	init_items();
	fill(&head, 6);
	list_bulk_move_tail(&head, &items[1].node, &items[3].node);
	CHECK(list_matches(&head, want, 6));
}

TEST(splice)
{
	struct list_head a, b;
	static const int want1[] = { 3, 4, 0, 1, 2 };
	static const int want2[] = { 3, 4, 0, 1, 2, 5, 6 };
	int i;

	init_items();
	fill(&a, 3);
	INIT_LIST_HEAD(&b);
	list_add_tail(&items[3].node, &b);
	list_add_tail(&items[4].node, &b);
	list_splice_init(&b, &a);
	CHECK(list_empty(&b));
	CHECK(list_matches(&a, want1, 5));

	/* Splicing an empty list is a no-op. */
	list_splice(&b, &a);
	list_splice_tail(&b, &a);
	CHECK(list_matches(&a, want1, 5));

	for (i = 5; i < 7; i++)
		list_add_tail(&items[i].node, &b);
	list_splice_tail_init(&b, &a);
	CHECK(list_empty(&b));
	CHECK(list_matches(&a, want2, 7));
}

TEST(cut)
{
	struct list_head head, cut;
	static const int head_want[] = { 3, 4, 5 };
	static const int cut_want[] = { 0, 1, 2 };
	static const int head_want2[] = { 4, 5 };
	static const int cut_want2[] = { 3 };

	init_items();
	fill(&head, 6);
	list_cut_position(&cut, &head, &items[2].node);
	CHECK(list_matches(&head, head_want, 3));
	CHECK(list_matches(&cut, cut_want, 3));

	list_cut_before(&cut, &head, &items[4].node);
	CHECK(list_matches(&head, head_want2, 2));
	CHECK(list_matches(&cut, cut_want2, 1));

	/* Cutting at head yields an empty list and leaves head intact. */
	list_cut_position(&cut, &head, &head);
	CHECK(list_empty(&cut));
	CHECK(list_matches(&head, head_want2, 2));

	/* Cutting before the first entry yields an empty list. */
	list_cut_before(&cut, &head, &items[4].node);
	CHECK(list_empty(&cut));
	CHECK(list_matches(&head, head_want2, 2));
}

TEST(iterators)
{
	struct list_head head, *p, *n;
	struct item *it, *tmp;
	int sum = 0, count = 0;

	init_items();
	fill(&head, NITEMS);

	list_for_each(p, &head)
		count++;
	CHECK_EQ(count, NITEMS);

	count = 0;
	list_for_each_prev(p, &head)
		count++;
	CHECK_EQ(count, NITEMS);

	/* Safe iteration while deleting odd values. */
	list_for_each_entry_safe(it, tmp, &head, node) {
		if (it->val & 1)
			list_del_init(&it->node);
	}
	CHECK_EQ(list_count_nodes(&head), NITEMS / 2);
	list_for_each_entry(it, &head, node)
		sum += it->val;
	CHECK_EQ(sum, 0 + 2 + 4 + 6);

	/* continue / from start after a given entry. */
	it = &items[2];
	sum = 0;
	list_for_each_entry_continue(it, &head, node)
		sum += it->val;
	CHECK_EQ(sum, 4 + 6);

	it = &items[4];
	sum = 0;
	list_for_each_entry_from(it, &head, node)
		sum += it->val;
	CHECK_EQ(sum, 4 + 6);

	it = &items[4];
	sum = 0;
	list_for_each_entry_continue_reverse(it, &head, node)
		sum += it->val;
	CHECK_EQ(sum, 2 + 0);

	/* Safe raw iteration that empties the list. */
	list_for_each_safe(p, n, &head)
		list_del_init(p);
	CHECK(list_empty(&head));

	/* Reverse safe iteration. */
	fill(&head, 4);
	sum = 0;
	list_for_each_entry_safe_reverse(it, tmp, &head, node) {
		sum = sum * 10 + it->val;
		list_del(&it->node);
	}
	CHECK_EQ(sum, 3210);
	CHECK(list_empty(&head));

	fill(&head, 3);
	CHECK(list_next_entry(&items[0], node) == &items[1]);
	CHECK(list_prev_entry(&items[2], node) == &items[1]);
	CHECK(list_first_entry_or_null(&head, struct item, node) == &items[0]);
}

TEST(hlist)
{
	struct hlist_head h;
	struct hlist_node *p, *n;
	struct item *it;
	int sum = 0;
	int order[4], k = 0;

	init_items();
	INIT_HLIST_HEAD(&h);
	CHECK(hlist_empty(&h));
	CHECK(hlist_unhashed(&items[0].hnode));

	hlist_add_head(&items[0].hnode, &h);
	hlist_add_head(&items[1].hnode, &h);		/* 1 0 */
	hlist_add_behind(&items[2].hnode, &items[1].hnode); /* 1 2 0 */
	hlist_add_before(&items[3].hnode, &items[1].hnode); /* 3 1 2 0 */
	CHECK(!hlist_empty(&h));
	CHECK(!hlist_unhashed(&items[0].hnode));

	hlist_for_each_entry(it, &h, hnode)
		order[k++] = it->val;
	CHECK_EQ(k, 4);
	CHECK(order[0] == 3 && order[1] == 1 && order[2] == 2 && order[3] == 0);

	hlist_del_init(&items[1].hnode);		/* 3 2 0 */
	CHECK(hlist_unhashed(&items[1].hnode));
	hlist_del_init(&items[1].hnode);		/* no-op when unhashed */
	hlist_del(&items[3].hnode);			/* 2 0 */
	CHECK(h.first == &items[2].hnode);
	CHECK(items[2].hnode.pprev == &h.first);

	hlist_for_each(p, &h)
		sum += hlist_entry(p, struct item, hnode)->val;
	CHECK_EQ(sum, 2);

	{
		struct hlist_head h2;

		INIT_HLIST_HEAD(&h2);
		hlist_move_list(&h, &h2);
		CHECK(hlist_empty(&h));
		CHECK(h2.first == &items[2].hnode);
		CHECK(items[2].hnode.pprev == &h2.first);

		hlist_for_each_safe(p, n, &h2)
			hlist_del_init(p);
		CHECK(hlist_empty(&h2));
	}

	{
		struct item *tmpit;
		struct hlist_node *nn;
		int count = 0;

		hlist_add_head(&items[5].hnode, &h);
		hlist_add_head(&items[6].hnode, &h);
		hlist_for_each_entry_safe(tmpit, nn, &h, hnode) {
			hlist_del(&tmpit->hnode);
			count++;
		}
		CHECK_EQ(count, 2);
		CHECK(hlist_empty(&h));
	}
}

LKPI_SUITE(list)
{
	RUN(container_of);
	RUN(static_head);
	RUN(add_and_add_tail);
	RUN(del);
	RUN(replace_and_swap);
	RUN(move_and_rotate);
	RUN(bulk_move_tail);
	RUN(splice);
	RUN(cut);
	RUN(iterators);
	RUN(hlist);
}
