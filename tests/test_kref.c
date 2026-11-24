/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Unit tests for <linux/kref.h> and <linux/refcount.h>.
 */

/* Count KPI warnings instead of printing them (must precede linux/ headers). */
static int warn_count;
#define	lkpi_warn(msg)	((void)(msg), warn_count++)

#include <linux/kref.h>
#include <linux/kernel.h>

#include "harness.h"

struct obj {
	int		payload;
	struct kref	ref;
};

static int released;
static struct obj *last_released;

static void
obj_release(struct kref *kref)
{
	released++;
	last_released = container_of(kref, struct obj, ref);
}

TEST(init_get_put)
{
	struct obj o = { .payload = 42 };

	released = 0;
	warn_count = 0;
	kref_init(&o.ref);
	CHECK_EQ(kref_read(&o.ref), 1);

	kref_get(&o.ref);
	kref_get(&o.ref);
	CHECK_EQ(kref_read(&o.ref), 3);

	CHECK_EQ(kref_put(&o.ref, obj_release), 0);
	CHECK_EQ(kref_put(&o.ref, obj_release), 0);
	CHECK_EQ(released, 0);
	CHECK_EQ(kref_put(&o.ref, obj_release), 1);
	CHECK_EQ(released, 1);
	CHECK(last_released == &o);
	CHECK_EQ(kref_read(&o.ref), 0);
	CHECK_EQ(warn_count, 0);
}

TEST(static_init)
{
	struct kref k = KREF_INIT(2);
	refcount_t r = REFCOUNT_INIT(5);

	CHECK_EQ(kref_read(&k), 2);
	CHECK_EQ(refcount_read(&r), 5);
}

TEST(get_unless_zero)
{
	struct obj o;

	released = 0;
	warn_count = 0;
	kref_init(&o.ref);
	CHECK(kref_get_unless_zero(&o.ref));
	CHECK_EQ(kref_read(&o.ref), 2);
	CHECK_EQ(kref_put(&o.ref, obj_release), 0);
	CHECK_EQ(kref_put(&o.ref, obj_release), 1);

	/* Dead object: must not be revived. */
	CHECK(!kref_get_unless_zero(&o.ref));
	CHECK_EQ(kref_read(&o.ref), 0);
	CHECK_EQ(released, 1);
	CHECK_EQ(warn_count, 0);
}

TEST(get_on_zero_saturates)
{
	struct obj o;

	released = 0;
	warn_count = 0;
	kref_init(&o.ref);
	CHECK_EQ(kref_put(&o.ref, obj_release), 1);

	/* Use-after-free style kref_get: warns and saturates, never frees. */
	kref_get(&o.ref);
	CHECK_EQ(warn_count, 1);
	CHECK_EQ((int)kref_read(&o.ref), REFCOUNT_SATURATED);
	CHECK_EQ(kref_put(&o.ref, obj_release), 0);
	CHECK_EQ(released, 1);
}

TEST(underflow_saturates)
{
	refcount_t r = REFCOUNT_INIT(0);

	warn_count = 0;
	CHECK(!refcount_dec_and_test(&r));
	CHECK_EQ(warn_count, 1);
	CHECK_EQ((int)refcount_read(&r), REFCOUNT_SATURATED);
}

TEST(overflow_saturates)
{
	refcount_t r = REFCOUNT_INIT(REFCOUNT_MAX);

	warn_count = 0;
	refcount_inc(&r);
	CHECK_EQ(warn_count, 1);
	CHECK_EQ((int)refcount_read(&r), REFCOUNT_SATURATED);

	/* Saturated counters stay saturated-ish and never hit zero. */
	CHECK(!refcount_dec_and_test(&r));
}

TEST(refcount_misc)
{
	refcount_t r;

	warn_count = 0;
	refcount_set(&r, 1);
	CHECK(!refcount_dec_not_one(&r));
	CHECK_EQ(refcount_read(&r), 1);
	refcount_add(3, &r);
	CHECK_EQ(refcount_read(&r), 4);
	CHECK(refcount_dec_not_one(&r));
	CHECK_EQ(refcount_read(&r), 3);
	refcount_dec(&r);
	CHECK_EQ(refcount_read(&r), 2);
	CHECK(refcount_add_not_zero(2, &r));
	CHECK_EQ(refcount_read(&r), 4);
	CHECK(refcount_sub_and_test(4, &r));
	CHECK(!refcount_inc_not_zero(&r));
	CHECK_EQ(warn_count, 0);

	/* refcount_dec to zero is a bug: warns, leaks. */
	refcount_set(&r, 1);
	refcount_dec(&r);
	CHECK_EQ(warn_count, 1);
	CHECK_EQ((int)refcount_read(&r), REFCOUNT_SATURATED);
}

/* Concurrency smoke test: exactly one release after racing get/put. */
#define	NTHREADS	8
#define	NITER		100000

static struct obj shared;

static void
hammer(void *arg)
{
	int i;

	(void)arg;
	for (i = 0; i < NITER; i++) {
		kref_get(&shared.ref);
		if (kref_get_unless_zero(&shared.ref))
			(void)kref_put(&shared.ref, obj_release);
		(void)kref_put(&shared.ref, obj_release);
	}
	/* Drop the reference main() took on our behalf. */
	(void)kref_put(&shared.ref, obj_release);
}

TEST(concurrent)
{
	int i;

	released = 0;
	warn_count = 0;
	kref_init(&shared.ref);
	for (i = 0; i < NTHREADS; i++)
		kref_get(&shared.ref);
	harness_run_threads(NTHREADS, hammer, NULL);

	CHECK_EQ(kref_read(&shared.ref), 1);
	CHECK_EQ(released, 0);
	CHECK_EQ(kref_put(&shared.ref, obj_release), 1);
	CHECK_EQ(released, 1);
	CHECK_EQ(warn_count, 0);
}

LKPI_SUITE(kref)
{
	RUN(init_get_put);
	RUN(static_init);
	RUN(get_unless_zero);
	RUN(get_on_zero_saturates);
	RUN(underflow_saturates);
	RUN(overflow_saturates);
	RUN(refcount_misc);
	RUN(concurrent);
}
