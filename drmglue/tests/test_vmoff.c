/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Unit tests for the interval tree (<drmglue/itree.h>) against a brute
 * force model, and for the fake-offset manager (<drmglue/vmoff.h>).
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <drmglue/itree.h>
#include <drmglue/vmoff.h>

#include "harness.h"

#define	N	300

static struct lkpi_itree_node nodes[N];
static bool in[N];
static unsigned int rng = 4242;

static unsigned long
rnd(unsigned long mod)
{
	rng = rng * 1103515245U + 12345U;
	return (((rng >> 8) & 0xffffff) % mod);
}

static void
check_query(struct lkpi_itree *t, unsigned long a, unsigned long b)
{
	struct lkpi_itree_node *it;
	int want = 0, got = 0, i;
	unsigned long prev = 0;

	for (i = 0; i < N; i++)
		if (in[i] && nodes[i].start <= b && nodes[i].last >= a)
			want++;
	for (it = lkpi_itree_first(t, a, b); it != NULL;
	    it = lkpi_itree_next(it, a, b)) {
		CHECK(it->start <= b && it->last >= a);
		CHECK(it->start >= prev);
		prev = it->start;
		got++;
	}
	CHECK_EQ(got, want);
}

TEST(itree_random)
{
	struct lkpi_itree t;
	int round, i, q;

	lkpi_itree_init(&t);
	for (round = 0; round < 4000; round++) {
		i = (int)rnd(N);
		if (in[i]) {
			lkpi_itree_remove(&t, &nodes[i]);
			in[i] = false;
		} else {
			nodes[i].start = rnd(1000);
			nodes[i].last = nodes[i].start + rnd(rnd(4) == 0 ?
			    300 : 10);
			lkpi_itree_insert(&t, &nodes[i]);
			in[i] = true;
		}
		if (round % 37 == 0) {
			for (q = 0; q < 8; q++) {
				unsigned long a = rnd(1100);

				check_query(&t, a, a + rnd(50));
			}
			check_query(&t, 0, ~0UL);
		}
	}
	for (i = 0; i < N; i++)
		if (in[i])
			lkpi_itree_remove(&t, &nodes[i]);
	CHECK(lkpi_itree_empty(&t));
	CHECK_EQ(t.count, 0);
}

TEST(vmoff_alloc_lookup)
{
	struct lkpi_vmoff_mgr m;
	struct lkpi_vmoff_node a, b, c, d;

	lkpi_vmoff_mgr_init(&m, 0x100000, 64);
	lkpi_vmoff_node_init(&a);
	lkpi_vmoff_node_init(&b);
	lkpi_vmoff_node_init(&c);
	lkpi_vmoff_node_init(&d);

	CHECK_EQ(lkpi_vmoff_add(&m, &a, 0), -EINVAL);
	CHECK_EQ(lkpi_vmoff_add(&m, &a, 16), 0);
	CHECK_EQ(lkpi_vmoff_node_start(&a), 0x100000);
	CHECK_EQ(lkpi_vmoff_add(&m, &a, 16), 0);	/* idempotent */
	CHECK_EQ(lkpi_vmoff_add(&m, &b, 32), 0);
	CHECK_EQ(lkpi_vmoff_node_start(&b), 0x100010);
	CHECK_EQ(lkpi_vmoff_add(&m, &c, 17), -ENOSPC);
	CHECK_EQ(lkpi_vmoff_add(&m, &c, 16), 0);
	CHECK_EQ(lkpi_vmoff_node_offset_addr(&c, 12),
	    (uint64_t)0x100030 << 12);

	lkpi_vmoff_lock_lookup(&m);
	CHECK(lkpi_vmoff_lookup_locked(&m, 0x100000, 16) == &a);
	CHECK(lkpi_vmoff_lookup_locked(&m, 0x100005, 1) == &a);
	CHECK(lkpi_vmoff_lookup_locked(&m, 0x10000f, 2) == NULL); /* spans */
	CHECK(lkpi_vmoff_lookup_locked(&m, 0x100020, 4) == &b);
	CHECK(lkpi_vmoff_lookup_locked(&m, 0x0fffff, 1) == NULL);
	CHECK(lkpi_vmoff_lookup_locked(&m, 0x100040, 1) == NULL);
	lkpi_vmoff_unlock_lookup(&m);

	/* A freed hole is reused first-fit. */
	lkpi_vmoff_remove(&m, &a);
	CHECK(!lkpi_vmoff_node_has_offset(&a));
	CHECK_EQ(lkpi_vmoff_add(&m, &d, 8), 0);
	CHECK_EQ(lkpi_vmoff_node_start(&d), 0x100000);
	lkpi_vmoff_lock_lookup(&m);
	CHECK(lkpi_vmoff_lookup_locked(&m, 0x100009, 1) == NULL);
	lkpi_vmoff_unlock_lookup(&m);

	lkpi_vmoff_remove(&m, &b);
	lkpi_vmoff_remove(&m, &c);
	lkpi_vmoff_remove(&m, &d);
	lkpi_vmoff_remove(&m, &d);	/* harmless */
	lkpi_vmoff_node_fini(&a);
	lkpi_vmoff_node_fini(&b);
	lkpi_vmoff_node_fini(&c);
	lkpi_vmoff_node_fini(&d);
	lkpi_vmoff_mgr_destroy(&m);
}

TEST(vmoff_tags)
{
	struct lkpi_vmoff_node n;
	int f1, f2;

	lkpi_vmoff_node_init(&n);
	CHECK(!lkpi_vmoff_is_allowed(&n, &f1));
	CHECK_EQ(lkpi_vmoff_allow(&n, &f1), 0);
	CHECK_EQ(lkpi_vmoff_allow(&n, &f1), 0);
	CHECK_EQ(lkpi_vmoff_allow(&n, &f2), 0);
	CHECK(lkpi_vmoff_is_allowed(&n, &f1));
	lkpi_vmoff_revoke(&n, &f1);
	CHECK(lkpi_vmoff_is_allowed(&n, &f1));	/* counted */
	lkpi_vmoff_revoke(&n, &f1);
	CHECK(!lkpi_vmoff_is_allowed(&n, &f1));
	CHECK(lkpi_vmoff_is_allowed(&n, &f2));
	lkpi_vmoff_revoke(&n, &f1);		/* not present: no-op */
	lkpi_vmoff_node_fini(&n);		/* frees f2's tag */
}

LKPI_SUITE(drmglue_vmoff)
{
	RUN(itree_random);
	RUN(vmoff_alloc_lookup);
	RUN(vmoff_tags);
}
