/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Unit tests for <linux/atomic.h> and <asm/barrier.h>.
 */

#include <linux/atomic.h>
#include <linux/kernel.h>
#include <asm/barrier.h>

#include "harness.h"

TEST(atomic_basic)
{
	atomic_t a = ATOMIC_INIT(5);

	CHECK_EQ(atomic_read(&a), 5);
	atomic_set(&a, 10);
	CHECK_EQ(atomic_read(&a), 10);
	atomic_inc(&a);
	atomic_add(4, &a);
	CHECK_EQ(atomic_read(&a), 15);
	atomic_dec(&a);
	atomic_sub(4, &a);
	CHECK_EQ(atomic_read(&a), 10);
	CHECK_EQ(atomic_inc_return(&a), 11);
	CHECK_EQ(atomic_dec_return(&a), 10);
	CHECK_EQ(atomic_add_return(5, &a), 15);
	CHECK_EQ(atomic_sub_return(5, &a), 10);
	CHECK_EQ(atomic_fetch_add(2, &a), 10);
	CHECK_EQ(atomic_fetch_sub(2, &a), 12);
	CHECK_EQ(atomic_fetch_inc(&a), 10);
	CHECK_EQ(atomic_fetch_dec(&a), 11);
	CHECK_EQ(atomic_read(&a), 10);
	CHECK_EQ(atomic_add_return_relaxed(1, &a), 11);
	CHECK_EQ(atomic_fetch_add_acquire(1, &a), 11);
	CHECK_EQ(atomic_fetch_sub_release(2, &a), 12);
	CHECK_EQ(atomic_read_acquire(&a), 10);
	atomic_set_release(&a, 0);
	CHECK_EQ(atomic_read(&a), 0);
}

TEST(atomic_bitwise)
{
	atomic_t a = ATOMIC_INIT(0x0f);

	atomic_or(0x30, &a);
	CHECK_EQ(atomic_read(&a), 0x3f);
	atomic_and(0x3c, &a);
	CHECK_EQ(atomic_read(&a), 0x3c);
	atomic_andnot(0x0c, &a);
	CHECK_EQ(atomic_read(&a), 0x30);
	atomic_xor(0x11, &a);
	CHECK_EQ(atomic_read(&a), 0x21);
	CHECK_EQ(atomic_fetch_or(0x02, &a), 0x21);
	CHECK_EQ(atomic_fetch_and(0x03, &a), 0x23);
	CHECK_EQ(atomic_fetch_andnot(0x01, &a), 0x03);
	CHECK_EQ(atomic_fetch_xor(0x03, &a), 0x02);
	CHECK_EQ(atomic_read(&a), 0x01);
}

TEST(atomic_tests)
{
	atomic_t a = ATOMIC_INIT(2);

	CHECK(!atomic_dec_and_test(&a));
	CHECK(atomic_dec_and_test(&a));
	atomic_set(&a, -1);
	CHECK(atomic_inc_and_test(&a));
	CHECK(atomic_add_negative(-1, &a));
	CHECK(!atomic_add_negative(1, &a));
	atomic_set(&a, 3);
	CHECK(atomic_sub_and_test(3, &a));
}

TEST(atomic_cmpxchg)
{
	atomic_t a = ATOMIC_INIT(7);
	int old;

	CHECK_EQ(atomic_cmpxchg(&a, 7, 9), 7);
	CHECK_EQ(atomic_read(&a), 9);
	CHECK_EQ(atomic_cmpxchg(&a, 7, 11), 9);
	CHECK_EQ(atomic_read(&a), 9);
	CHECK_EQ(atomic_xchg(&a, 1), 9);
	CHECK_EQ(atomic_read(&a), 1);

	old = 1;
	CHECK(atomic_try_cmpxchg(&a, &old, 2));
	CHECK_EQ(old, 1);
	old = 5;
	CHECK(!atomic_try_cmpxchg(&a, &old, 3));
	CHECK_EQ(old, 2);
	CHECK_EQ(atomic_read(&a), 2);
	CHECK_EQ(atomic_cmpxchg_relaxed(&a, 2, 4), 2);
	CHECK_EQ(atomic_cmpxchg_acquire(&a, 4, 5), 4);
	CHECK_EQ(atomic_cmpxchg_release(&a, 5, 6), 5);
	CHECK_EQ(atomic_xchg_relaxed(&a, 0), 6);
}

TEST(atomic_conditional)
{
	atomic_t a = ATOMIC_INIT(0);

	CHECK(!atomic_inc_not_zero(&a));
	CHECK_EQ(atomic_read(&a), 0);
	atomic_set(&a, 1);
	CHECK(atomic_inc_not_zero(&a));
	CHECK_EQ(atomic_read(&a), 2);
	CHECK(!atomic_add_unless(&a, 1, 2));
	CHECK(atomic_add_unless(&a, 1, 5));
	CHECK_EQ(atomic_fetch_add_unless(&a, 10, 3), 3);
	CHECK_EQ(atomic_read(&a), 3);

	CHECK_EQ(atomic_dec_if_positive(&a), 2);
	atomic_set(&a, 0);
	CHECK_EQ(atomic_dec_if_positive(&a), -1);
	CHECK_EQ(atomic_read(&a), 0);

	CHECK(atomic_inc_unless_negative(&a));
	atomic_set(&a, -1);
	CHECK(!atomic_inc_unless_negative(&a));
	CHECK(atomic_dec_unless_positive(&a));
	CHECK_EQ(atomic_read(&a), -2);
	atomic_set(&a, 1);
	CHECK(!atomic_dec_unless_positive(&a));
}

TEST(atomic64_long)
{
	atomic64_t a = ATOMIC64_INIT(0x100000000LL);
	atomic_long_t l = ATOMIC_LONG_INIT(-5);

	CHECK(atomic64_read(&a) == 0x100000000LL);
	CHECK(atomic64_add_return(0x100000000LL, &a) == 0x200000000LL);
	CHECK(atomic64_cmpxchg(&a, 0x200000000LL, 1) == 0x200000000LL);
	CHECK_EQ(atomic64_read(&a), 1);
	CHECK(atomic64_dec_and_test(&a));
	atomic64_or(1LL << 40, &a);
	CHECK(atomic64_fetch_and(1LL << 40, &a) == 1LL << 40);

	CHECK_EQ(atomic_long_read(&l), -5);
	atomic_long_add(10, &l);
	CHECK_EQ(atomic_long_inc_return(&l), 6);
	CHECK_EQ(atomic_long_xchg(&l, 0x7fffffffffffL), 6);
	CHECK(atomic_long_read(&l) == 0x7fffffffffffL);
	CHECK_EQ(sizeof(atomic_long_t), sizeof(long));
	CHECK_EQ(sizeof(atomic64_t), 8);
	CHECK_EQ(sizeof(atomic_t), 4);
}

TEST(generic_xchg)
{
	unsigned long ul = 3;
	u8 b = 1;
	u16 h = 0xffff;
	int x = 1, y = 2;
	int *p = &x;
	int *expect;

	CHECK_EQ(xchg(&ul, 4UL), 3);
	CHECK_EQ(ul, 4);
	CHECK_EQ(cmpxchg(&ul, 4UL, 5UL), 4);
	CHECK_EQ(cmpxchg(&ul, 4UL, 6UL), 5);
	CHECK_EQ(ul, 5);
	CHECK_EQ(cmpxchg(&b, 1, 2), 1);
	CHECK_EQ(b, 2);
	CHECK_EQ(xchg(&h, 7), 0xffff);
	CHECK_EQ(h, 7);
	CHECK(cmpxchg(&p, &x, &y) == &x);
	CHECK(p == &y);
	expect = &x;
	CHECK(!try_cmpxchg(&p, &expect, NULL));
	CHECK(expect == &y);
	CHECK(try_cmpxchg(&p, &expect, NULL));
	CHECK(p == NULL);
	CHECK_EQ(xchg_relaxed(&ul, 1UL), 5);
	CHECK_EQ(cmpxchg_relaxed(&ul, 1UL, 2UL), 1);
	CHECK_EQ(cmpxchg64(&ul, 2UL, 3UL), 2);
}

TEST(barriers)
{
	int v = 0;
	int r;

	/* These must compile and not crash; ordering is checked below. */
	mb();
	rmb();
	wmb();
	smp_mb();
	smp_rmb();
	smp_wmb();
	smp_mb__before_atomic();
	smp_mb__after_atomic();
	dma_rmb();
	dma_wmb();
	barrier();
	smp_store_mb(v, 3);
	CHECK_EQ(v, 3);
	smp_store_release(&v, 4);
	CHECK_EQ(smp_load_acquire(&v), 4);
	WRITE_ONCE(v, 5);
	CHECK_EQ(READ_ONCE(v), 5);
	r = smp_cond_load_relaxed(&v, VAL == 5);
	CHECK_EQ(r, 5);
	r = smp_cond_load_acquire(&v, VAL > 1);
	CHECK_EQ(r, 5);
}

/* Concurrency: lost updates would show up as a wrong final count. */
#define	NTHREADS	8
#define	NITER		100000

static atomic_t shared32;
static atomic64_t shared64;
static atomic_long_t sharedlong;
static unsigned long sharedcas;

static void
hammer(void *arg)
{
	unsigned long old;
	int i;

	(void)arg;
	for (i = 0; i < NITER; i++) {
		atomic_inc(&shared32);
		(void) atomic64_add_return(2, &shared64);
		atomic_long_dec(&sharedlong);
		old = READ_ONCE(sharedcas);
		while (!try_cmpxchg(&sharedcas, &old, old + 1))
			continue;
	}
}

TEST(concurrent)
{
	atomic_set(&shared32, 0);
	atomic64_set(&shared64, 0);
	atomic_long_set(&sharedlong, 0);
	sharedcas = 0;
	harness_run_threads(NTHREADS, hammer, NULL);
	CHECK_EQ(atomic_read(&shared32), NTHREADS * NITER);
	CHECK_EQ(atomic64_read(&shared64), 2LL * NTHREADS * NITER);
	CHECK_EQ(atomic_long_read(&sharedlong), -(long)NTHREADS * NITER);
	CHECK_EQ(sharedcas, NTHREADS * NITER);
}

/* Message passing with release/acquire: data must be visible with flag. */
static int mp_data;
static int mp_flag;
static atomic_t mp_errors;

static void
mp_consumer(void *arg)
{
	int i;

	(void)arg;
	for (i = 1; i <= 2000; i++) {
		while (smp_load_acquire(&mp_flag) != i)
			lkpi_cpu_relax();
		if (READ_ONCE(mp_data) != i * 3)
			atomic_inc(&mp_errors);
		smp_store_release(&mp_flag, -i);
	}
}

static void
mp_producer(void *arg)
{
	int i;

	(void)arg;
	for (i = 1; i <= 2000; i++) {
		WRITE_ONCE(mp_data, i * 3);
		smp_store_release(&mp_flag, i);
		while (smp_load_acquire(&mp_flag) != -i)
			lkpi_cpu_relax();
	}
}

static void
mp_thread(void *arg)
{
	static atomic_t which;

	(void)arg;
	if (atomic_fetch_inc(&which) % 2 == 0)
		mp_producer(NULL);
	else
		mp_consumer(NULL);
}

TEST(message_passing)
{
	mp_data = 0;
	mp_flag = 0;
	atomic_set(&mp_errors, 0);
	harness_run_threads(2, mp_thread, NULL);
	CHECK_EQ(atomic_read(&mp_errors), 0);
}

LKPI_SUITE(atomic)
{
	RUN(atomic_basic);
	RUN(atomic_bitwise);
	RUN(atomic_tests);
	RUN(atomic_cmpxchg);
	RUN(atomic_conditional);
	RUN(atomic64_long);
	RUN(generic_xchg);
	RUN(barriers);
	RUN(concurrent);
	RUN(message_passing);
}
