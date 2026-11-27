/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Unit tests for spinlock_t, rwlock_t, struct mutex, struct rw_semaphore,
 * seqcount/seqlock, completions, lockdep-lite and kref_put_lock/_mutex.
 * In userland the locks are the pthread-backed shims of
 * <linuxkpi/platform_userland.h>.
 */

#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/rwsem.h>
#include <linux/seqlock.h>
#include <linux/completion.h>
#include <linux/lockdep.h>
#include <linux/kref.h>
#include <linux/jiffies.h>

#include "harness.h"

static void
quiet(int level, const char *msg)
{
	(void) level;
	(void) msg;
}

static DEFINE_SPINLOCK(static_spin);
static DEFINE_MUTEX(static_mutex);
static DECLARE_RWSEM(static_rwsem);
static DEFINE_RWLOCK(static_rwlock);
static DEFINE_SEQLOCK(static_seqlock);
static DECLARE_COMPLETION(static_done);

TEST(spinlock)
{
	spinlock_t l;
	unsigned long flags;

	spin_lock_init(&l);
	CHECK(!spin_is_locked(&l));
	spin_lock(&l);
	CHECK(spin_is_locked(&l));
	CHECK(lkpi_spin_held(&l));
	CHECK(lockdep_is_held(&l));
	spin_unlock(&l);
	CHECK(!lockdep_is_held(&l));
	CHECK(spin_trylock(&l));
	CHECK(spin_is_locked(&l));
	spin_unlock(&l);
	spin_lock_irqsave(&l, flags);
	CHECK_EQ(flags, 0);
	spin_unlock_irqrestore(&l, flags);
	spin_lock_irq(&l);
	spin_unlock_irq(&l);
	spin_lock_bh(&l);
	spin_unlock_bh(&l);
	CHECK(spin_trylock_irqsave(&l, flags));
	spin_unlock_irqrestore(&l, flags);
	lkpi_spin_lock_destroy(&l);

	spin_lock(&static_spin);
	assert_spin_locked(&static_spin);
	spin_unlock(&static_spin);

	/* IRQ-safe initialisation takes an iblock cookie (ignored here). */
	spin_lock_init_irqsafe(&l, NULL);
	spin_lock(&l);
	spin_unlock(&l);
	lkpi_spin_lock_destroy(&l);
}

TEST(mutex)
{
	struct mutex m;

	mutex_init(&m);
	CHECK(!mutex_is_locked(&m));
	mutex_lock(&m);
	CHECK(mutex_is_locked(&m));
	CHECK(lockdep_is_held(&m));
	lockdep_assert_held(&m);
	mutex_unlock(&m);
	CHECK(mutex_trylock(&m));
	CHECK(!lockdep_is_held(&static_mutex));
	mutex_unlock(&m);
	CHECK_EQ(mutex_lock_interruptible(&m), 0);
	mutex_unlock(&m);
	CHECK_EQ(mutex_lock_killable(&m), 0);
	mutex_unlock(&m);
	mutex_lock_nested(&m, 1);
	mutex_unlock(&m);
	mutex_destroy(&m);

	mutex_lock(&static_mutex);
	CHECK(mutex_is_locked(&static_mutex));
	mutex_unlock(&static_mutex);
}

/* docs/12 L5: interruptible/killable acquisition. */
static struct mutex sig_m;
static volatile int sig_held, sig_release;

static void
sig_holder(void *arg)
{
	(void) arg;
	mutex_lock(&sig_m);
	__atomic_store_n(&sig_held, 1, __ATOMIC_SEQ_CST);
	while (!__atomic_load_n(&sig_release, __ATOMIC_SEQ_CST))
		lkpi_plat_sleep_ns(1000000);
	mutex_unlock(&sig_m);
}

TEST(mutex_interruptible)
{
	struct lkpi_plat_thread *t;

	mutex_init(&sig_m);
	sig_held = sig_release = 0;
	t = lkpi_plat_thread_create(sig_holder, NULL, "holder");
	while (!__atomic_load_n(&sig_held, __ATOMIC_SEQ_CST))
		lkpi_plat_sleep_ns(1000000);
#ifndef LKPI_KTEST
	lkpi_plat_fake_signal = 1;
	CHECK_EQ(mutex_lock_interruptible(&sig_m), -EINTR);
	lkpi_plat_fake_signal = 2;		/* fatal */
	CHECK_EQ(mutex_lock_killable(&sig_m), -EINTR);
	lkpi_plat_fake_signal = 0;
#endif
	CHECK_EQ(sig_m.lkpi_waiters, 0);
	__atomic_store_n(&sig_release, 1, __ATOMIC_SEQ_CST);
	CHECK_EQ(mutex_lock_interruptible(&sig_m), 0);	/* after release */
	mutex_unlock(&sig_m);
	lkpi_plat_thread_join(t);
	mutex_destroy(&sig_m);
}

TEST(rwsem)
{
	struct rw_semaphore s;

	init_rwsem(&s);
	CHECK(!rwsem_is_locked(&s));
	down_read(&s);
	CHECK(rwsem_is_locked(&s));
	CHECK(down_read_trylock(&s));
	CHECK(!down_write_trylock(&s));
	lockdep_assert_held_read(&s);
	up_read(&s);
	up_read(&s);
	CHECK(!rwsem_is_locked(&s));

	down_write(&s);
	CHECK(lkpi_rwsem_write_held(&s));
	lockdep_assert_held_write(&s);
	CHECK(!down_read_trylock(&s));
	downgrade_write(&s);
	CHECK(!lkpi_rwsem_write_held(&s));
	CHECK(lkpi_rwsem_read_held(&s));
	CHECK(down_read_trylock(&s));
	up_read(&s);
	up_read(&s);
	CHECK(down_write_trylock(&s));
	up_write(&s);
	CHECK_EQ(down_read_interruptible(&s), 0);
	up_read(&s);
	CHECK_EQ(down_write_killable(&s), 0);
	up_write(&s);
	lkpi_rwsem_destroy(&s);

	down_write(&static_rwsem);
	up_write(&static_rwsem);
}

TEST(rwlock)
{
	rwlock_t l;
	unsigned long flags;

	rwlock_init(&l);
	read_lock(&l);
	CHECK(read_trylock(&l));
	CHECK(!write_trylock(&l));
	CHECK(lockdep_is_held(&l));
	read_unlock(&l);
	read_unlock(&l);
	write_lock_irqsave(&l, flags);
	CHECK(lkpi_rwlock_write_held(&l));
	CHECK(!read_trylock(&l));
	write_unlock_irqrestore(&l, flags);
	CHECK(write_trylock(&l));
	write_unlock(&l);
	read_lock(&static_rwlock);
	read_unlock(&static_rwlock);
}

TEST(lockdep_lite)
{
	struct mutex m;
	spinlock_t sp;
	struct rw_semaphore s;
	unsigned int before;

	mutex_init(&m);
	spin_lock_init(&sp);
	init_rwsem(&s);
	lkpi_printk_hook = quiet;
	before = linuxkpi_warn_count;

	/* Satisfied assertions are silent. */
	mutex_lock(&m);
	lockdep_assert_held(&m);
	mutex_unlock(&m);
	lockdep_assert_not_held(&m);
	spin_lock(&sp);
	lockdep_assert_held(&sp);
	spin_unlock(&sp);
	down_write(&s);
	lockdep_assert_held_write(&s);
	lockdep_assert_held(&s);
	up_write(&s);
	lockdep_assert(1 == 1);
	CHECK_EQ(linuxkpi_warn_count, before);

	/* Each violated assertion warns (once per call site). */
	lockdep_assert_held(&m);
	CHECK_EQ(linuxkpi_warn_count, before + 1);
	lockdep_assert_held(&sp);
	CHECK_EQ(linuxkpi_warn_count, before + 2);
	lockdep_assert_held_write(&s);
	CHECK_EQ(linuxkpi_warn_count, before + 3);
	lockdep_assert_held_read(&s);
	CHECK_EQ(linuxkpi_warn_count, before + 4);
	mutex_lock(&m);
	lockdep_assert_not_held(&m);
	CHECK_EQ(linuxkpi_warn_count, before + 5);
	mutex_unlock(&m);

	/* Annotations compile and do nothing. */
	{
		static struct lock_class_key key;

		lockdep_set_class(&m, &key);
		might_lock(&m);
		lockdep_assert_irqs_disabled();
	}
	lkpi_printk_hook = NULL;
	mutex_destroy(&m);
	lkpi_spin_lock_destroy(&sp);
	lkpi_rwsem_destroy(&s);
}

struct obj {
	struct kref	ref;
	int		released;
};

static void
obj_release_unlock_mutex(struct kref *k)
{
	struct obj *o = container_of(k, struct obj, ref);

	CHECK(mutex_is_locked(&static_mutex));
	o->released = 1;
	mutex_unlock(&static_mutex);
}

static void
obj_release_unlock_spin(struct kref *k)
{
	struct obj *o = container_of(k, struct obj, ref);

	CHECK(spin_is_locked(&static_spin));
	o->released = 1;
	spin_unlock(&static_spin);
}

TEST(kref_put_locked)
{
	struct obj o = { .released = 0 };
	atomic_t a = ATOMIC_INIT(2);
	refcount_t r = REFCOUNT_INIT(2);

	kref_init(&o.ref);
	kref_get(&o.ref);
	CHECK_EQ(kref_put_mutex(&o.ref, obj_release_unlock_mutex,
	    &static_mutex), 0);
	CHECK(!mutex_is_locked(&static_mutex));
	CHECK_EQ(kref_put_mutex(&o.ref, obj_release_unlock_mutex,
	    &static_mutex), 1);
	CHECK_EQ(o.released, 1);
	CHECK(!mutex_is_locked(&static_mutex));

	o.released = 0;
	kref_init(&o.ref);
	CHECK_EQ(kref_put_lock(&o.ref, obj_release_unlock_spin,
	    &static_spin), 1);
	CHECK_EQ(o.released, 1);
	CHECK(!spin_is_locked(&static_spin));

	CHECK(!atomic_dec_and_lock(&a, &static_spin));
	CHECK(atomic_dec_and_lock(&a, &static_spin));
	CHECK(spin_is_locked(&static_spin));
	spin_unlock(&static_spin);
	atomic_set(&a, 1);
	CHECK(atomic_dec_and_mutex_lock(&a, &static_mutex));
	mutex_unlock(&static_mutex);
	CHECK(!refcount_dec_and_mutex_lock(&r, &static_mutex));
	CHECK(refcount_dec_and_mutex_lock(&r, &static_mutex));
	mutex_unlock(&static_mutex);
	refcount_set(&r, 1);
	CHECK(refcount_dec_and_lock(&r, &static_spin));
	spin_unlock(&static_spin);
}

TEST(seqcount_basic)
{
	seqcount_t sc = SEQCNT_ZERO(sc);
	seqcount_spinlock_t scs;
	spinlock_t l;
	unsigned int seq;
	int i, retries = 0;

	seq = read_seqcount_begin(&sc);
	CHECK_EQ(seq, 0);
	CHECK(!read_seqcount_retry(&sc, seq));
	write_seqcount_begin(&sc);
	CHECK_EQ(raw_read_seqcount(&sc) & 1, 1);
	CHECK_EQ(raw_seqcount_begin(&sc), 0);
	write_seqcount_end(&sc);
	CHECK(read_seqcount_retry(&sc, seq));
	write_seqcount_invalidate(&sc);
	CHECK_EQ(raw_read_seqcount(&sc), 4);

	spin_lock_init(&l);
	seqcount_spinlock_init(&scs, &l);
	seq = read_seqcount_begin(&scs);
	spin_lock(&l);
	write_seqcount_begin(&scs);
	write_seqcount_end(&scs);
	spin_unlock(&l);
	CHECK(read_seqcount_retry(&scs, seq));

	/* seqlock_t and the lock-or-retry pattern. */
	seq = read_seqbegin(&static_seqlock);
	write_seqlock(&static_seqlock);
	write_sequnlock(&static_seqlock);
	CHECK(read_seqretry(&static_seqlock, seq));
	for (i = 0; i < 2; i++) {
		int s = i;	/* 0: lockless, 1: locked */

		read_seqbegin_or_lock(&static_seqlock, &s);
		if (need_seqretry(&static_seqlock, s))
			retries++;
		done_seqretry(&static_seqlock, s);
	}
	CHECK_EQ(retries, 0);
	CHECK(!spin_is_locked(&static_seqlock.lock));
	lkpi_spin_lock_destroy(&l);
}

TEST(completion_basic)
{
	struct completion c;

	init_completion(&c);
	CHECK(!completion_done(&c));
	CHECK(!try_wait_for_completion(&c));
	complete(&c);
	complete(&c);
	CHECK(completion_done(&c));
	wait_for_completion(&c);
	CHECK(try_wait_for_completion(&c));
	CHECK(!completion_done(&c));

	/* Timeout expires: returns 0 after roughly the timeout. */
	{
		unsigned long t0 = jiffies;

		CHECK_EQ(wait_for_completion_timeout(&c, msecs_to_jiffies(30)),
		    0);
		CHECK(time_after_eq(jiffies, t0 + msecs_to_jiffies(30) - 1));
	}
	CHECK_EQ(wait_for_completion_timeout(&c, 0), 0);
	complete(&c);
	CHECK(wait_for_completion_timeout(&c, 0) >= 1);
	complete(&c);
	CHECK(wait_for_completion_timeout(&c, HZ) >= 1);
	complete(&c);
	CHECK_EQ(wait_for_completion_interruptible(&c), 0);
	CHECK_EQ(wait_for_completion_interruptible_timeout(&c, 1), 0);

	/* complete_all() releases every waiter, forever, until reinit. */
	complete_all(&c);
	wait_for_completion(&c);
	wait_for_completion(&c);
	CHECK(completion_done(&c));
	reinit_completion(&c);
	CHECK(!completion_done(&c));
	lkpi_completion_destroy(&c);

	complete(&static_done);
	wait_for_completion(&static_done);
}

/* Concurrency. */
#define	NTHREADS	8
#define	NITER		20000

static struct mutex cm;
static spinlock_t cs;
static long mutex_counter, spin_counter;

static void
lock_hammer(void *arg)
{
	int i;

	(void) arg;
	for (i = 0; i < NITER; i++) {
		mutex_lock(&cm);
		mutex_counter++;
		mutex_unlock(&cm);
		spin_lock(&cs);
		spin_counter++;
		spin_unlock(&cs);
	}
}

TEST(concurrent_mutex_spin)
{
	mutex_init(&cm);
	spin_lock_init(&cs);
	mutex_counter = spin_counter = 0;
	harness_run_threads(NTHREADS, lock_hammer, NULL);
	CHECK_EQ(mutex_counter, NTHREADS * NITER);
	CHECK_EQ(spin_counter, NTHREADS * NITER);
	mutex_destroy(&cm);
	lkpi_spin_lock_destroy(&cs);
}

static struct rw_semaphore crw;
static int rw_a, rw_b;
static atomic_t rw_errors;
static atomic_t rw_turn;

static void
rwsem_hammer(void *arg)
{
	int i, me = atomic_fetch_inc(&rw_turn);

	(void) arg;
	for (i = 0; i < NITER / 4; i++) {
		if (me == 0) {
			down_write(&crw);
			rw_a++;
			rw_b++;
			up_write(&crw);
		} else {
			down_read(&crw);
			if (rw_a != rw_b)
				atomic_inc(&rw_errors);
			up_read(&crw);
		}
	}
}

TEST(concurrent_rwsem)
{
	init_rwsem(&crw);
	rw_a = rw_b = 0;
	atomic_set(&rw_errors, 0);
	atomic_set(&rw_turn, 0);
	harness_run_threads(4, rwsem_hammer, NULL);
	CHECK_EQ(atomic_read(&rw_errors), 0);
	CHECK_EQ(rw_a, NITER / 4);
	lkpi_rwsem_destroy(&crw);
}

/* A seqlock writer keeps a == b; readers must never see a torn pair. */
static seqlock_t csl;
static volatile long sq_a, sq_b;
static atomic_t sq_turn, sq_torn, sq_stop;

static void
seq_hammer(void *arg)
{
	int me = atomic_fetch_inc(&sq_turn);
	unsigned int seq;
	long a, b;
	int i;

	(void) arg;
	if (me == 0) {
		for (i = 0; i < NITER * 2; i++) {
			write_seqlock(&csl);
			sq_a = i;
			sq_b = i;
			write_sequnlock(&csl);
		}
		atomic_set(&sq_stop, 1);
		return;
	}
	while (!atomic_read(&sq_stop)) {
		do {
			seq = read_seqbegin(&csl);
			a = sq_a;
			b = sq_b;
		} while (read_seqretry(&csl, seq));
		if (a != b)
			atomic_inc(&sq_torn);
	}
}

TEST(concurrent_seqlock)
{
	seqlock_init(&csl);
	atomic_set(&sq_turn, 0);
	atomic_set(&sq_torn, 0);
	atomic_set(&sq_stop, 0);
	harness_run_threads(3, seq_hammer, NULL);
	CHECK_EQ(atomic_read(&sq_torn), 0);
	lkpi_spin_lock_destroy(&csl.lock);
}

/* Waiters woken by complete() one at a time and by complete_all(). */
static struct completion cc_go, cc_done;
static atomic_t cc_woken;

static void
completion_waiter(void *arg)
{
	(void) arg;
	wait_for_completion(&cc_go);
	atomic_inc(&cc_woken);
	complete(&cc_done);
}

TEST(concurrent_completion)
{
	int i;

	init_completion(&cc_go);
	init_completion(&cc_done);
	atomic_set(&cc_woken, 0);
	/* Two rounds: 4 waiters via complete() x4, then via complete_all(). */
	for (i = 0; i < 4; i++)
		complete(&cc_go);
	harness_run_threads(4, completion_waiter, NULL);
	CHECK_EQ(atomic_read(&cc_woken), 4);
	for (i = 0; i < 4; i++)
		CHECK(wait_for_completion_timeout(&cc_done, 10 * HZ) > 0);
	complete_all(&cc_go);
	harness_run_threads(4, completion_waiter, NULL);
	CHECK_EQ(atomic_read(&cc_woken), 8);
	lkpi_completion_destroy(&cc_go);
	lkpi_completion_destroy(&cc_done);
}

LKPI_SUITE(locks)
{
	RUN(spinlock);
	RUN(mutex);
	RUN(mutex_interruptible);
	RUN(rwsem);
	RUN(rwlock);
	RUN(lockdep_lite);
	RUN(kref_put_locked);
	RUN(seqcount_basic);
	RUN(completion_basic);
	RUN(concurrent_mutex_spin);
	RUN(concurrent_rwsem);
	RUN(concurrent_seqlock);
	RUN(concurrent_completion);
}
