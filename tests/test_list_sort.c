/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Unit tests for list_sort().
 */

#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/list_sort.h>

#include "harness.h"

struct item {
	int			key;
	int			seq;	/* input position, for stability */
	struct list_head	node;
};

static int ncmp;

static int
item_cmp(void *priv, const struct list_head *a, const struct list_head *b)
{
	const struct item *x = list_entry(a, struct item, node);
	const struct item *y = list_entry(b, struct item, node);

	ncmp++;
	CHECK(priv == &ncmp);
	return (x->key > y->key) - (x->key < y->key);
}

/* Sorted by key, stable, and prev links consistent. */
static bool
check_sorted(struct list_head *head, int n)
{
	struct item *it, *prev = NULL;
	struct list_head *p;
	int count = 0;

	list_for_each_entry(it, head, node) {
		if (prev != NULL && (prev->key > it->key ||
		    (prev->key == it->key && prev->seq > it->seq)))
			return (false);
		prev = it;
		count++;
	}
	for (p = head->prev; p != head; p = p->prev)
		count--;
	if (head->next->prev != head || head->prev->next != head)
		return (false);
	return (count == 0 && list_count_nodes(head) == (size_t)n);
}

static unsigned int rng = 12345;

static int
rnd(void)
{
	rng = rng * 1103515245U + 12345U;
	return ((int)((rng >> 16) & 0x7fff));
}

#define	MAXN	1000
static struct item items[MAXN];

static void
fill(struct list_head *head, int n, int mod)
{
	int i;

	INIT_LIST_HEAD(head);
	for (i = 0; i < n; i++) {
		items[i].key = mod > 0 ? rnd() % mod : -i;
		items[i].seq = i;
		list_add_tail(&items[i].node, head);
	}
}

TEST(small)
{
	LIST_HEAD(head);
	int n;

	list_sort(&ncmp, &head, item_cmp);	/* empty */
	CHECK(list_empty(&head));
	for (n = 1; n <= 9; n++) {
		fill(&head, n, 5);
		list_sort(&ncmp, &head, item_cmp);
		CHECK(check_sorted(&head, n));
	}
}

TEST(random_and_stable)
{
	LIST_HEAD(head);
	int n;

	for (n = 10; n <= MAXN; n += 99) {
		fill(&head, n, 1000);
		list_sort(&ncmp, &head, item_cmp);
		CHECK(check_sorted(&head, n));
		fill(&head, n, 3);	/* many duplicates: stability */
		list_sort(&ncmp, &head, item_cmp);
		CHECK(check_sorted(&head, n));
	}
}

TEST(reversed_and_sorted_input)
{
	LIST_HEAD(head);
	int i;

	fill(&head, MAXN, 0);	/* strictly decreasing keys */
	ncmp = 0;
	list_sort(&ncmp, &head, item_cmp);
	CHECK(check_sorted(&head, MAXN));
	CHECK(ncmp < MAXN * 11);	/* O(n log n) */
	list_sort(&ncmp, &head, item_cmp);	/* already sorted */
	CHECK(check_sorted(&head, MAXN));
	i = 0;
	{
		struct item *it;

		list_for_each_entry(it, &head, node)
			CHECK_EQ(it->key, -(MAXN - 1) + i++);
	}
}

LKPI_SUITE(list_sort)
{
	RUN(small);
	RUN(random_and_stable);
	RUN(reversed_and_sorted_input);
}
