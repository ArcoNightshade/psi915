/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Unit tests for <linux/xarray.h> and <linux/idr.h> (IDR, IDA), including
 * a randomised comparison against a flat reference array and a leak check
 * of the tree nodes via lkpi_kmalloc_live.
 */

#include <linux/kernel.h>
#include <linux/xarray.h>
#include <linux/idr.h>
#include <linux/slab.h>

#include "harness.h"

static long live0;
static int vals[4096];

static void
quiet(int level, const char *msg)
{
	(void) level;
	(void) msg;
}

TEST(xa_basic)
{
	struct xarray xa;
	unsigned long idx;
	void *e;
	int n;

	live0 = atomic_long_read(&lkpi_kmalloc_live);
	xa_init(&xa);
	CHECK(xa_empty(&xa));
	CHECK(xa_load(&xa, 0) == NULL);
	CHECK(xa_store(&xa, 5, &vals[5], GFP_KERNEL) == NULL);
	CHECK(xa_load(&xa, 5) == &vals[5]);
	CHECK(xa_store(&xa, 5, &vals[6], GFP_KERNEL) == &vals[5]);
	CHECK(xa_store(&xa, 1000000, &vals[7], GFP_KERNEL) == NULL);
	CHECK(xa_store(&xa, ULONG_MAX, xa_mk_value(42), GFP_KERNEL) == NULL);
	CHECK(xa_is_value(xa_load(&xa, ULONG_MAX)));
	CHECK_EQ(xa_to_value(xa_load(&xa, ULONG_MAX)), 42);
	CHECK(xa_load(&xa, 1000001) == NULL);
	CHECK(xa_load(&xa, 4) == NULL);

	n = 0;
	xa_for_each(&xa, idx, e)
		n++;
	CHECK_EQ(n, 3);
	idx = 6;
	CHECK(xa_find(&xa, &idx, ULONG_MAX, XA_PRESENT) == &vals[7]);
	CHECK_EQ(idx, 1000000);
	CHECK(xa_find_after(&xa, &idx, ULONG_MAX, XA_PRESENT) != NULL);
	CHECK(idx == ULONG_MAX);
	CHECK(xa_find_after(&xa, &idx, ULONG_MAX, XA_PRESENT) == NULL);
	n = 0;
	xa_for_each_range(&xa, idx, e, 5, 1000000)
		n++;
	CHECK_EQ(n, 2);

	CHECK(xa_erase(&xa, 5) == &vals[6]);
	CHECK(xa_erase(&xa, 5) == NULL);
	CHECK(xa_store(&xa, 9, NULL, GFP_KERNEL) == NULL);	/* erase */
	CHECK(xa_insert(&xa, 1000000, &vals[1], GFP_KERNEL) == -EBUSY);
	CHECK_EQ(xa_insert(&xa, 77, &vals[1], GFP_KERNEL), 0);
	CHECK(xa_cmpxchg(&xa, 77, &vals[2], &vals[3], GFP_KERNEL) == &vals[1]);
	CHECK(xa_load(&xa, 77) == &vals[1]);
	CHECK(xa_cmpxchg(&xa, 77, &vals[1], &vals[3], GFP_KERNEL) == &vals[1]);
	CHECK(xa_load(&xa, 77) == &vals[3]);
	CHECK(xa_is_err(xa_store(&xa, 3, xa_mk_internal(3), GFP_KERNEL)));
	CHECK_EQ(xa_err(xa_store(&xa, 3, xa_mk_internal(3), GFP_KERNEL)),
	    -EINVAL);
	CHECK_EQ(xa_err(&vals[0]), 0);
	xa_erase(&xa, 77);
	xa_erase(&xa, 1000000);
	xa_erase(&xa, ULONG_MAX);
	CHECK(xa_empty(&xa));		/* nodes pruned */
	CHECK_EQ(atomic_long_read(&lkpi_kmalloc_live), live0);
}

TEST(xa_reserve_and_lock)
{
	DEFINE_XARRAY(xa);
	unsigned long idx;
	void *e;
	int n = 0;

	CHECK_EQ(xa_reserve(&xa, 10, GFP_KERNEL), 0);
	CHECK(xa_load(&xa, 10) == NULL);
	CHECK(!xa_empty(&xa));
	CHECK(xa_insert(&xa, 10, &vals[0], GFP_KERNEL) == -EBUSY);
	xa_for_each(&xa, idx, e)
		n++;
	CHECK_EQ(n, 0);			/* reserved entries are skipped */
	CHECK(xa_cmpxchg(&xa, 10, NULL, &vals[1], GFP_KERNEL) == NULL);
	CHECK(xa_load(&xa, 10) == &vals[1]);
	xa_erase(&xa, 10);
	CHECK_EQ(xa_reserve(&xa, 11, GFP_KERNEL), 0);
	xa_release(&xa, 11);
	CHECK(xa_empty(&xa));

	/* Calls with xa_lock held (the __ forms, and xa_load). */
	xa_lock(&xa);
	CHECK(__xa_store(&xa, 3, &vals[3], GFP_ATOMIC) == NULL);
	CHECK(xa_load(&xa, 3) == &vals[3]);
	CHECK_EQ(__xa_insert(&xa, 4, &vals[4], GFP_ATOMIC), 0);
	CHECK(__xa_cmpxchg(&xa, 4, &vals[4], &vals[5], GFP_ATOMIC) == &vals[4]);
	CHECK(__xa_erase(&xa, 3) == &vals[3]);
	CHECK(xa_store(&xa, 6, &vals[6], GFP_ATOMIC) == NULL);
	xa_unlock(&xa);
	xa_destroy(&xa);
	CHECK(xa_empty(&xa));
	CHECK_EQ(atomic_long_read(&lkpi_kmalloc_live), live0);
}

TEST(xa_alloc)
{
	DEFINE_XARRAY_ALLOC(xa);
	DEFINE_XARRAY_ALLOC1(xa1);
	u32 id, next = 0;
	int i;

	for (i = 0; i < 200; i++) {
		CHECK_EQ(xa_alloc(&xa, &id, &vals[i], xa_limit_32b,
		    GFP_KERNEL), 0);
		CHECK_EQ(id, i);
	}
	xa_erase(&xa, 70);
	xa_erase(&xa, 3);
	CHECK_EQ(xa_alloc(&xa, &id, &vals[0], xa_limit_32b, GFP_KERNEL), 0);
	CHECK_EQ(id, 3);
	CHECK_EQ(xa_alloc(&xa, &id, NULL, xa_limit_32b, GFP_KERNEL), 0);
	CHECK_EQ(id, 70);		/* reserved */
	CHECK(xa_load(&xa, 70) == NULL);
	CHECK_EQ(xa_alloc(&xa, &id, &vals[0], xa_limit_32b, GFP_KERNEL), 0);
	CHECK_EQ(id, 200);
	CHECK(xa_store(&xa, 70, NULL, GFP_KERNEL) == NULL);	/* stays busy */
	CHECK_EQ(xa_alloc(&xa, &id, &vals[0], XA_LIMIT(60, 80), GFP_KERNEL),
	    -EBUSY);			/* 60..80 all busy, 70 reserved */
	CHECK_EQ(xa_alloc(&xa, &id, &vals[0], XA_LIMIT(5, 6), GFP_KERNEL),
	    -EBUSY);
	CHECK_EQ(xa_alloc(&xa, &id, &vals[0], XA_LIMIT(1000000, 2000000),
	    GFP_KERNEL), 0);
	CHECK_EQ(id, 1000000);
	CHECK_EQ(xa_alloc(&xa, &id, &vals[0], XA_LIMIT(UINT_MAX, UINT_MAX),
	    GFP_KERNEL), 0);
	CHECK(id == UINT_MAX);
	xa_destroy(&xa);

	CHECK_EQ(xa_alloc(&xa1, &id, &vals[0], xa_limit_32b, GFP_KERNEL), 0);
	CHECK_EQ(id, 1);		/* ALLOC1: 0 is never handed out */

	/* Cyclic: continues after the last id, wraps once. */
	for (i = 0; i < 3; i++)
		CHECK_EQ(xa_alloc_cyclic(&xa, &id, &vals[0], XA_LIMIT(0, 3),
		    &next, GFP_KERNEL), 0);
	CHECK_EQ(id, 2);
	xa_erase(&xa, 1);
	CHECK_EQ(xa_alloc_cyclic(&xa, &id, &vals[0], XA_LIMIT(0, 3), &next,
	    GFP_KERNEL), 0);
	CHECK_EQ(id, 3);
	CHECK_EQ(xa_alloc_cyclic(&xa, &id, &vals[0], XA_LIMIT(0, 3), &next,
	    GFP_KERNEL), 1);		/* wrapped */
	CHECK_EQ(id, 1);
	CHECK_EQ(xa_alloc_cyclic(&xa, &id, &vals[0], XA_LIMIT(0, 3), &next,
	    GFP_KERNEL), -EBUSY);
	xa_destroy(&xa);
	xa_destroy(&xa1);
	CHECK_EQ(atomic_long_read(&lkpi_kmalloc_live), live0);
}

static unsigned int rng = 4242;

static unsigned long
rnd(unsigned long mod)
{
	rng = rng * 1103515245U + 12345U;
	return (((rng >> 8) ^ (rng << 13)) % mod);
}

#define	REFN	4096

TEST(xa_random_vs_reference)
{
	static void *ref[REFN];
	DEFINE_XARRAY(xa);
	unsigned long idx, scale, expect_idx;
	void *e;
	int i, j, op, mismatches = 0;

	for (i = 0; i < REFN; i++)
		ref[i] = NULL;
	for (i = 0; i < 20000; i++) {
		j = (int)rnd(REFN);
		scale = (unsigned long)j * 977;	/* spread indices out */
		op = (int)rnd(3);
		if (op == 0) {
			if (xa_store(&xa, scale, &vals[j], GFP_KERNEL) != ref[j])
				mismatches++;
			ref[j] = &vals[j];
		} else if (op == 1) {
			if (xa_erase(&xa, scale) != ref[j])
				mismatches++;
			ref[j] = NULL;
		} else if (xa_load(&xa, scale) != ref[j]) {
			mismatches++;
		}
	}
	CHECK_EQ(mismatches, 0);
	/* Iteration yields exactly the reference entries, in order. */
	expect_idx = 0;
	j = 0;
	xa_for_each(&xa, idx, e) {
		while (j < REFN && ref[j] == NULL)
			j++;
		CHECK(j < REFN);
		if (j >= REFN)
			break;
		CHECK_EQ(idx, (unsigned long)j * 977);
		CHECK(e == ref[j]);
		expect_idx = idx;
		j++;
	}
	while (j < REFN && ref[j] == NULL)
		j++;
	CHECK_EQ(j, REFN);
	(void) expect_idx;
	for (i = 0; i < REFN; i++)
		xa_erase(&xa, (unsigned long)i * 977);
	CHECK(xa_empty(&xa));
	CHECK_EQ(atomic_long_read(&lkpi_kmalloc_live), live0);
}

static int
sum_ids(int id, void *p, void *data)
{
	*(int *)data += id;
	return (p == &vals[99] ? 99 : 0);
}

TEST(idr)
{
	DEFINE_IDR(idr);
	struct idr based;
	void *e;
	int id, i, sum = 0;
	u32 uid;

	CHECK(idr_is_empty(&idr));
	for (i = 0; i < 10; i++)
		CHECK_EQ(idr_alloc(&idr, &vals[i], 0, 0, GFP_KERNEL), i);
	CHECK(idr_find(&idr, 4) == &vals[4]);
	CHECK(idr_remove(&idr, 4) == &vals[4]);
	CHECK(idr_find(&idr, 4) == NULL);
	CHECK_EQ(idr_alloc(&idr, &vals[40], 0, 0, GFP_KERNEL), 4);
	CHECK_EQ(idr_alloc(&idr, &vals[0], 100, 102, GFP_KERNEL), 100);
	CHECK_EQ(idr_alloc(&idr, &vals[0], 100, 102, GFP_KERNEL), 101);
	CHECK_EQ(idr_alloc(&idr, &vals[0], 100, 102, GFP_KERNEL), -ENOSPC);
	CHECK_EQ(idr_alloc(&idr, NULL, 50, 0, GFP_KERNEL), 50);	/* reserve */
	CHECK(idr_find(&idr, 50) == NULL);
	CHECK(idr_replace(&idr, &vals[50], 50) == NULL);
	CHECK(idr_find(&idr, 50) == &vals[50]);
	CHECK(idr_replace(&idr, &vals[51], 50) == &vals[50]);
	CHECK(IS_ERR(idr_replace(&idr, &vals[0], 60)));
	CHECK_EQ(PTR_ERR(idr_replace(&idr, &vals[0], 60)), -ENOENT);

	id = 0;
	i = 0;
	idr_for_each_entry(&idr, e, id)
		i++;
	CHECK_EQ(i, 13);
	id = 11;
	CHECK(idr_get_next(&idr, &id) == &vals[51]);
	CHECK_EQ(id, 50);
	CHECK_EQ(idr_for_each(&idr, sum_ids, &sum), 0);
	CHECK_EQ(sum, 45 + 50 + 100 + 101);
	idr_replace(&idr, &vals[99], 7);
	sum = 0;
	CHECK_EQ(idr_for_each(&idr, sum_ids, &sum), 99);	/* stops early */

	uid = 1000;
	CHECK_EQ(idr_alloc_u32(&idr, &vals[0], &uid, 2000, GFP_KERNEL), 0);
	CHECK_EQ(uid, 1000);
	idr_destroy(&idr);
	CHECK(idr_is_empty(&idr));

	/* Cyclic allocation keeps moving forward. */
	CHECK_EQ(idr_alloc_cyclic(&idr, &vals[0], 1, 4, GFP_KERNEL), 1);
	CHECK_EQ(idr_alloc_cyclic(&idr, &vals[0], 1, 4, GFP_KERNEL), 2);
	idr_remove(&idr, 1);
	CHECK_EQ(idr_alloc_cyclic(&idr, &vals[0], 1, 4, GFP_KERNEL), 3);
	CHECK_EQ(idr_alloc_cyclic(&idr, &vals[0], 1, 4, GFP_KERNEL), 1);
	CHECK_EQ(idr_alloc_cyclic(&idr, &vals[0], 1, 4, GFP_KERNEL), -ENOSPC);
	idr_destroy(&idr);

	idr_init_base(&based, 1);
	CHECK_EQ(idr_alloc(&based, &vals[1], 0, 0, GFP_KERNEL), 1);
	CHECK(idr_find(&based, 1) == &vals[1]);
	CHECK(idr_find(&based, 0) == NULL);
	idr_destroy(&based);
	CHECK_EQ(atomic_long_read(&lkpi_kmalloc_live), live0);
}

TEST(ida)
{
	DEFINE_IDA(ida);
	unsigned int before = linuxkpi_warn_count;
	int i;

	CHECK(ida_is_empty(&ida));
	for (i = 0; i < 100; i++)
		CHECK_EQ(ida_alloc(&ida, GFP_KERNEL), i);
	ida_free(&ida, 42);
	CHECK_EQ(ida_alloc(&ida, GFP_KERNEL), 42);
	CHECK_EQ(ida_alloc_min(&ida, 1000, GFP_KERNEL), 1000);
	CHECK_EQ(ida_alloc_max(&ida, 99, GFP_KERNEL), -ENOSPC);
	CHECK_EQ(ida_alloc_range(&ida, 5, 5, GFP_KERNEL), -ENOSPC);
	CHECK_EQ(ida_simple_get(&ida, 2000, 2001, GFP_KERNEL), 2000);
	ida_simple_remove(&ida, 2000);
	lkpi_printk_hook = quiet;
	ida_free(&ida, 5000);			/* not allocated: warns */
	lkpi_printk_hook = NULL;
	CHECK_EQ(linuxkpi_warn_count, before + 1);
	ida_destroy(&ida);
	CHECK(ida_is_empty(&ida));
	CHECK_EQ(atomic_long_read(&lkpi_kmalloc_live), live0);
}

LKPI_SUITE(xarray)
{
	RUN(xa_basic);
	RUN(xa_reserve_and_lock);
	RUN(xa_alloc);
	RUN(xa_random_vs_reference);
	RUN(idr);
	RUN(ida);
}
