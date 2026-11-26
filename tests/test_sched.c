/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Unit tests for current/task_struct, schedule()/wake_up_process(), wait
 * queues and wait_event*(), var/bit waits, delays and ktime. In userland
 * a pending signal is simulated with lkpi_plat_fake_signal.
 */

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/ktime.h>
#include <linux/atomic.h>
#include <linux/hash.h>

#include "harness.h"

extern atomic_t lkpi_task_live;

static struct task_struct *seen_task;
static int seen_pid;

static void
current_thread(void *arg)
{
	struct task_struct *t = current;

	(void) arg;
	seen_task = t;
	seen_pid = t->pid;
}

TEST(current_identity)
{
	struct task_struct *a = current, *b = current;
	int live;

	CHECK(a != NULL);
	CHECK(a == b);
	CHECK(a->__state == TASK_RUNNING);
	CHECK(task_is_running(a));
	CHECK(refcount_read(&a->usage) >= 1);
	CHECK_EQ(task_pid_nr(a), a->pid);
	CHECK(a->comm[0] != '\0');
	get_task_struct(a);
	put_task_struct(a);
	CHECK(!signal_pending(a));
	CHECK(!fatal_signal_pending(a));
	CHECK(in_task());
	CHECK(!in_interrupt());

	live = atomic_read(&lkpi_task_live);
	harness_run_threads(1, current_thread, NULL);
	CHECK(seen_task != NULL);
	CHECK(seen_task != a);
	CHECK_EQ(seen_pid, a->pid);
#ifndef LKPI_KTEST
	/* The thread's task was freed by the TSD destructor at exit. */
	CHECK_EQ(atomic_read(&lkpi_task_live), live);
#else
	(void) live;
#endif
}

/* A sleeper that waits for `go` with the raw protocol. */
static struct task_struct *sleeper_task;
static int sleeper_go;
static int sleeper_done;

static void
sleeper(void *arg)
{
	(void) arg;
	WRITE_ONCE(sleeper_task, get_task_struct(current));
	for (;;) {
		set_current_state(TASK_UNINTERRUPTIBLE);
		if (READ_ONCE(sleeper_go))
			break;
		schedule();
	}
	__set_current_state(TASK_RUNNING);
	WRITE_ONCE(sleeper_done, 1);
}

static void
waker(void *arg)
{
	struct task_struct *t;

	(void) arg;
	while ((t = READ_ONCE(sleeper_task)) == NULL)
		lkpi_plat_sleep_ns(100000);
	lkpi_plat_sleep_ns(5000000);
	WRITE_ONCE(sleeper_go, 1);
	(void) wake_up_process(t);
}

static void
sched_pair(void *arg)
{
	if (arg == NULL)
		sleeper(NULL);
	else
		waker(NULL);
}

static atomic_t pair_role;

static void
sched_role(void *arg)
{
	(void) arg;
	sched_pair(atomic_inc_return(&pair_role) == 1 ? NULL : (void *)1);
}

TEST(schedule_wake)
{
	sleeper_task = NULL;
	sleeper_go = sleeper_done = 0;
	atomic_set(&pair_role, 0);
	harness_run_threads(2, sched_role, NULL);
	CHECK_EQ(sleeper_done, 1);
	if (sleeper_task != NULL) {
		/* Not sleeping: a wake-up is a no-op. */
		CHECK_EQ(wake_up_process(sleeper_task), 0);
		put_task_struct(sleeper_task);
	}
}

TEST(schedule_timeout)
{
	unsigned long t0 = jiffies;
	long left;

	left = schedule_timeout_uninterruptible(msecs_to_jiffies(30));
	CHECK_EQ(left, 0);
	CHECK(time_after_eq(jiffies, t0 + msecs_to_jiffies(30)));
	CHECK(task_is_running(current));

	/* A task already woken does not sleep. */
	set_current_state(TASK_INTERRUPTIBLE);
	CHECK_EQ(wake_up_process(current), 1);
	left = schedule_timeout(msecs_to_jiffies(1000));
	CHECK(left > 0);
	/* TASK_RUNNING: schedule() returns at once. */
	schedule();
}

static DECLARE_WAIT_QUEUE_HEAD(static_wq);
static struct wait_queue_head wq;
static atomic_t wq_flag;
static atomic_t wq_woken;

static void
wq_waiter(void *arg)
{
	(void) arg;
	wait_event(wq, atomic_read(&wq_flag) != 0);
	atomic_inc(&wq_woken);
}

static void
wq_excl_waiter(void *arg)
{
	(void) arg;
	if (wait_event_interruptible_exclusive(wq,
	    atomic_add_unless(&wq_flag, -1, 0)) == 0)
		atomic_inc(&wq_woken);
}

TEST(wait_event_basic)
{
	long r;

	init_waitqueue_head(&wq);
	CHECK(!waitqueue_active(&wq));
	CHECK(!wq_has_sleeper(&static_wq));
	atomic_set(&wq_flag, 0);
	atomic_set(&wq_woken, 0);

	/* Condition already true: no sleep. */
	wait_event(static_wq, 1);
	CHECK_EQ(wait_event_interruptible(static_wq, 1), 0);
	CHECK_EQ(wait_event_timeout(static_wq, 1, 0), 1);
	CHECK_EQ(wait_event_timeout(static_wq, 1, 50), 50);
	r = wait_event_timeout(static_wq, 0, msecs_to_jiffies(20));
	CHECK_EQ(r, 0);
	r = wait_event_interruptible_timeout(static_wq, 0, 1);
	CHECK_EQ(r, 0);

	wake_up(&wq);	/* nobody waiting */
	CHECK_EQ(__wake_up(&wq, TASK_NORMAL, 0, NULL), 0);
	lkpi_waitqueue_destroy(&wq);
}

static void
wq_driver(void *arg)
{
	long mode = (long)arg;
	int i;

	/* Let the waiters block first; correctness does not depend on it. */
	lkpi_plat_sleep_ns(10000000);
	if (mode == 0) {
		atomic_set(&wq_flag, 1);
		wake_up_all(&wq);
	} else {
		for (i = 0; i < 4; i++) {
			atomic_inc(&wq_flag);
			wake_up(&wq);
			lkpi_plat_sleep_ns(1000000);
		}
	}
}

static atomic_t wq_role;

static void
wq_all_role(void *arg)
{
	if (atomic_inc_return(&wq_role) == 1)
		wq_driver((void *)0);
	else
		wq_waiter(arg);
}

static void
wq_excl_role(void *arg)
{
	if (atomic_inc_return(&wq_role) == 1)
		wq_driver((void *)1);
	else
		wq_excl_waiter(arg);
}

TEST(wait_event_threads)
{
	init_waitqueue_head(&wq);
	atomic_set(&wq_flag, 0);
	atomic_set(&wq_woken, 0);
	atomic_set(&wq_role, 0);
	harness_run_threads(5, wq_all_role, NULL);
	CHECK_EQ(atomic_read(&wq_woken), 4);
	CHECK(!waitqueue_active(&wq));

	/* Exclusive: four tokens, four waiters, each takes exactly one. */
	atomic_set(&wq_flag, 0);
	atomic_set(&wq_woken, 0);
	atomic_set(&wq_role, 0);
	harness_run_threads(5, wq_excl_role, NULL);
	CHECK_EQ(atomic_read(&wq_woken), 4);
	CHECK_EQ(atomic_read(&wq_flag), 0);
	lkpi_waitqueue_destroy(&wq);
}

TEST(wait_signals)
{
	DEFINE_WAIT(w);
	long r;

	init_waitqueue_head(&wq);
#ifndef LKPI_KTEST
	lkpi_plat_fake_signal = 1;
	CHECK(signal_pending(current));
	CHECK(!fatal_signal_pending(current));
	CHECK_EQ(wait_event_interruptible(wq, 0), -ERESTARTSYS);
	r = wait_event_interruptible_timeout(wq, 0, msecs_to_jiffies(1000));
	CHECK_EQ(r, -ERESTARTSYS);
	/* Uninterruptible waits ignore signals. */
	CHECK_EQ(wait_event_timeout(wq, 0, msecs_to_jiffies(15)), 0);
	/* Killable: non-fatal signals do not end the wait. */
	CHECK_EQ(wait_event_killable_timeout(wq, 0, msecs_to_jiffies(25)), 0);
	CHECK(schedule_timeout_interruptible(msecs_to_jiffies(1000)) >
	    (long)msecs_to_jiffies(1000) - 5);
	lkpi_plat_fake_signal = 2;
	CHECK(fatal_signal_pending(current));
	CHECK_EQ(wait_event_killable(wq, 0), -ERESTARTSYS);
	CHECK(msleep_interruptible(1000) > 0);
	lkpi_plat_fake_signal = 0;
#endif
	/* Raw prepare/finish without a waker. */
	prepare_to_wait(&wq, &w, TASK_INTERRUPTIBLE);
	CHECK(waitqueue_active(&wq));
	finish_wait(&wq, &w);
	CHECK(!waitqueue_active(&wq));
	CHECK(task_is_running(current));
	r = prepare_to_wait_event(&wq, &w, TASK_UNINTERRUPTIBLE);
	CHECK_EQ(r, 0);
	(void) schedule_timeout(1);
	finish_wait(&wq, &w);
	lkpi_waitqueue_destroy(&wq);
}

static unsigned long var_word;
static int var_value;

static void
var_thread(void *arg)
{
	(void) arg;
	lkpi_plat_sleep_ns(5000000);
	WRITE_ONCE(var_value, 1);
	wake_up_var(&var_value);
	lkpi_plat_sleep_ns(5000000);
	clear_and_wake_up_bit(3, &var_word);
}

static atomic_t var_role;

static void
var_role_fn(void *arg)
{
	if (atomic_inc_return(&var_role) == 1) {
		var_thread(arg);
		return;
	}
	wait_var_event(&var_value, READ_ONCE(var_value) != 0);
	CHECK_EQ(wait_on_bit(&var_word, 3, TASK_UNINTERRUPTIBLE), 0);
	CHECK(!test_bit(3, &var_word));
}

TEST(var_and_bit_waits)
{
	var_value = 0;
	var_word = 0;
	set_bit(3, &var_word);
	atomic_set(&var_role, 0);
	harness_run_threads(2, var_role_fn, NULL);
	CHECK_EQ(var_value, 1);
	CHECK(__var_waitqueue(&var_value) == __var_waitqueue(&var_value));
	set_bit(1, &var_word);
	CHECK_EQ(wait_on_bit_timeout(&var_word, 1, TASK_UNINTERRUPTIBLE, 2),
	    -EAGAIN);
	CHECK_EQ(wait_on_bit(&var_word, 2, TASK_UNINTERRUPTIBLE), 0);
}

TEST(delays_and_ktime)
{
	ktime_t t0, t1;
	struct timespec64 ts;

	t0 = ktime_get();
	udelay(200);
	t1 = ktime_get();
	CHECK(ktime_us_delta(t1, t0) >= 200);

	t0 = ktime_get();
	usleep_range(500, 1000);
	CHECK(ktime_us_delta(ktime_get(), t0) >= 500);
	t0 = ktime_get();
	msleep(12);
	CHECK(ktime_ms_delta(ktime_get(), t0) >= 12);
	t0 = ktime_get();
	fsleep(3000);
	CHECK(ktime_us_delta(ktime_get(), t0) >= 3000);
	ndelay(1500);
	mdelay(1);

	CHECK_EQ(ktime_to_ns(ktime_set(2, 5)), 2000000005LL);
	CHECK_EQ(ktime_to_us(ms_to_ktime(3)), 3000);
	CHECK_EQ(ktime_to_ms(us_to_ktime(4000)), 4);
	CHECK(ktime_after(ktime_add_ms(t0, 1), t0));
	CHECK(ktime_before(t0, ktime_add_us(t0, 1)));
	CHECK_EQ(ktime_compare(t0, t0), 0);
	ts = ktime_to_timespec64(ktime_set(3, 7));
	CHECK_EQ(ts.tv_sec, 3);
	CHECK_EQ(ts.tv_nsec, 7);
	ts = ns_to_timespec64(-1);
	CHECK_EQ(ts.tv_sec, -1);
	CHECK_EQ(ts.tv_nsec, NSEC_PER_SEC - 1);
	CHECK_EQ(timespec64_to_ktime(ts), -1);
	CHECK(ktime_get_real_seconds() > 1600000000LL);
	CHECK(ktime_get_ns() > 0);
	CHECK_EQ(ktime_divns(ktime_set(10, 0), NSEC_PER_SEC), 10);

	CHECK(hash_32(0x12345678, 8) < 256);
	CHECK(hash_64(0x123456789abcdefULL, 10) < 1024);
	CHECK(hash_ptr(&ts, 6) < 64);
	CHECK(hash_32(1, 16) != hash_32(2, 16));
}

LKPI_SUITE(sched)
{
	RUN(current_identity);
	RUN(schedule_wake);
	RUN(schedule_timeout);
	RUN(wait_event_basic);
	RUN(wait_event_threads);
	RUN(wait_signals);
	RUN(var_and_bit_waits);
	RUN(delays_and_ktime);
}
