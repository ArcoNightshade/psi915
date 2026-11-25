/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Unit tests for timer_list, hrtimer, workqueues (plain, delayed, rcu,
 * ordered, flush/cancel semantics), kthreads and kthread_worker.
 */

#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/hrtimer.h>
#include <linux/workqueue.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/atomic.h>
#include <linux/slab.h>
#include <linux/completion.h>
#include <linuxkpi/subsys.h>

#include "harness.h"

struct tctx {
	struct timer_list	t;
	atomic_t		fired;
	int			rearm;
};

static void
tctx_fn(struct timer_list *t)
{
	struct tctx *c = from_timer(c, t, t);

	atomic_inc(&c->fired);
	if (c->rearm > 0) {
		c->rearm--;
		(void) mod_timer(&c->t, jiffies + 1);
	}
}

TEST(timer_list_basic)
{
	struct tctx c;

	memset(&c, 0, sizeof (c));
	timer_setup(&c.t, tctx_fn, 0);
	CHECK(!timer_pending(&c.t));
	CHECK_EQ(mod_timer(&c.t, jiffies + msecs_to_jiffies(10)), 0);
	CHECK(timer_pending(&c.t));
	CHECK_EQ(mod_timer(&c.t, jiffies + msecs_to_jiffies(10)), 1);
	msleep(60);
	CHECK_EQ(atomic_read(&c.fired), 1);
	CHECK(!timer_pending(&c.t));

	/* Deleted before expiry: never fires. */
	(void) mod_timer(&c.t, jiffies + msecs_to_jiffies(50));
	CHECK_EQ(del_timer(&c.t), 1);
	CHECK_EQ(del_timer(&c.t), 0);
	msleep(80);
	CHECK_EQ(atomic_read(&c.fired), 1);

	/* Self re-arming timer stopped by del_timer_sync. */
	c.rearm = 1000000;
	(void) mod_timer(&c.t, jiffies);
	msleep(30);
	(void) del_timer_sync(&c.t);
	CHECK(!timer_pending(&c.t));
	{
		int n = atomic_read(&c.fired);

		CHECK(n > 1);
		msleep(30);
		CHECK_EQ(atomic_read(&c.fired), n);
	}

	/* timer_reduce only moves earlier; shutdown blocks re-arming. */
	c.rearm = 0;
	(void) mod_timer(&c.t, jiffies + msecs_to_jiffies(10000));
	CHECK_EQ(timer_reduce(&c.t, jiffies + msecs_to_jiffies(20000)), 1);
	CHECK(time_after(c.t.expires, jiffies + msecs_to_jiffies(5000)));
	(void) timer_reduce(&c.t, jiffies + 1);
	msleep(40);
	CHECK(!timer_pending(&c.t));
	CHECK_EQ(timer_shutdown_sync(&c.t), 0);
	CHECK_EQ(mod_timer(&c.t, jiffies + 1), 0);
	CHECK(!timer_pending(&c.t));
}

struct hctx {
	struct hrtimer	h;
	atomic_t	fired;
	int		periods;
};

static enum hrtimer_restart
hctx_fn(struct hrtimer *h)
{
	struct hctx *c = container_of(h, struct hctx, h);

	atomic_inc(&c->fired);
	if (--c->periods > 0) {
		(void) hrtimer_forward_now(h, us_to_ktime(500));
		return (HRTIMER_RESTART);
	}
	return (HRTIMER_NORESTART);
}

TEST(hrtimer_basic)
{
	struct hctx c;
	ktime_t t0;

	memset(&c, 0, sizeof (c));
	hrtimer_setup(&c.h, hctx_fn, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	c.periods = 1;
	t0 = ktime_get();
	hrtimer_start(&c.h, us_to_ktime(2000), HRTIMER_MODE_REL);
	CHECK(hrtimer_active(&c.h));
	CHECK(ktime_to_us(hrtimer_get_remaining(&c.h)) <= 2000);
	while (atomic_read(&c.fired) == 0 &&
	    ktime_ms_delta(ktime_get(), t0) < 1000)
		usleep_range(100, 200);
	CHECK_EQ(atomic_read(&c.fired), 1);
	CHECK(ktime_us_delta(ktime_get(), t0) >= 2000);
	CHECK(!hrtimer_active(&c.h));

	/* Periodic restart five times. */
	atomic_set(&c.fired, 0);
	c.periods = 5;
	hrtimer_start(&c.h, ktime_add_us(ktime_get(), 200), HRTIMER_MODE_ABS);
	msleep(50);
	CHECK_EQ(atomic_read(&c.fired), 5);

	/* Cancel before expiry. */
	c.periods = 1;
	hrtimer_start(&c.h, ms_to_ktime(100), HRTIMER_MODE_REL);
	CHECK_EQ(hrtimer_try_to_cancel(&c.h), 1);
	CHECK_EQ(hrtimer_cancel(&c.h), 0);
	hrtimer_start(&c.h, ms_to_ktime(100), HRTIMER_MODE_REL);
	CHECK_EQ(hrtimer_cancel(&c.h), 1);

	/* forward arithmetic */
	hrtimer_set_expires(&c.h, 1000);
	CHECK_EQ(hrtimer_forward(&c.h, 3500, 1000), 3);
	CHECK_EQ(hrtimer_get_expires(&c.h), 4000);
	CHECK_EQ(hrtimer_forward(&c.h, 3500, 1000), 0);
}

struct wctx {
	struct work_struct	w;
	struct delayed_work	dw;
	atomic_t		runs;
	atomic_t		active;
	atomic_t		overlap;
	int			sleep_ms;
};

static void
wctx_fn(struct work_struct *w)
{
	struct wctx *c = container_of(w, struct wctx, w);

	if (atomic_inc_return(&c->active) > 1)
		atomic_inc(&c->overlap);
	if (c->sleep_ms)
		msleep(c->sleep_ms);
	atomic_inc(&c->runs);
	atomic_dec(&c->active);
}

static void
wctx_dfn(struct work_struct *w)
{
	struct wctx *c = container_of(to_delayed_work(w), struct wctx, dw);

	atomic_inc(&c->runs);
}

TEST(work_basic)
{
	struct wctx c;
	int i;

	memset(&c, 0, sizeof (c));
	INIT_WORK(&c.w, wctx_fn);
	CHECK(!work_pending(&c.w));
	CHECK(queue_work(system_wq, &c.w));
	flush_work(&c.w);
	CHECK_EQ(atomic_read(&c.runs), 1);
	CHECK(!work_pending(&c.w));
	CHECK(!flush_work(&c.w));

	/* Queueing a pending item is a no-op. */
	c.sleep_ms = 20;
	CHECK(schedule_work(&c.w));
	msleep(5);		/* now running */
	CHECK(schedule_work(&c.w));	/* requeued while running */
	CHECK(!schedule_work(&c.w));
	flush_work(&c.w);
	CHECK_EQ(atomic_read(&c.runs), 3);
	CHECK_EQ(atomic_read(&c.overlap), 0);

	/* Non-reentrancy across many queue attempts on a multi-worker wq. */
	c.sleep_ms = 1;
	for (i = 0; i < 50; i++) {
		(void) queue_work(system_unbound_wq, &c.w);
		udelay(300);
	}
	flush_work(&c.w);
	CHECK_EQ(atomic_read(&c.overlap), 0);

	/* cancel_work_sync on pending and running items. */
	c.sleep_ms = 30;
	(void) queue_work(system_wq, &c.w);
	msleep(5);
	(void) queue_work(system_wq, &c.w);
	CHECK(cancel_work_sync(&c.w));
	CHECK(!work_pending(&c.w));
	CHECK_EQ(atomic_read(&c.active), 0);
	CHECK(!cancel_work_sync(&c.w));
}

TEST(delayed_work)
{
	struct wctx c;

	memset(&c, 0, sizeof (c));
	INIT_DELAYED_WORK(&c.dw, wctx_dfn);
	CHECK(queue_delayed_work(system_wq, &c.dw, msecs_to_jiffies(20)));
	CHECK(delayed_work_pending(&c.dw));
	CHECK(!queue_delayed_work(system_wq, &c.dw, 1));
	msleep(80);
	CHECK_EQ(atomic_read(&c.runs), 1);
	CHECK(!delayed_work_pending(&c.dw));

	/* Cancel before the timer fires. */
	(void) schedule_delayed_work(&c.dw, msecs_to_jiffies(50));
	CHECK(cancel_delayed_work(&c.dw));
	CHECK(!cancel_delayed_work(&c.dw));
	msleep(80);
	CHECK_EQ(atomic_read(&c.runs), 1);

	/* mod_delayed_work moves an armed timer. */
	(void) schedule_delayed_work(&c.dw, msecs_to_jiffies(10000));
	CHECK(mod_delayed_work(system_wq, &c.dw, 0));
	flush_delayed_work(&c.dw);
	CHECK_EQ(atomic_read(&c.runs), 2);
	CHECK(!mod_delayed_work(system_wq, &c.dw, msecs_to_jiffies(10000)));
	CHECK(flush_delayed_work(&c.dw));
	CHECK_EQ(atomic_read(&c.runs), 3);

	(void) schedule_delayed_work(&c.dw, msecs_to_jiffies(10000));
	CHECK(cancel_delayed_work_sync(&c.dw));
	CHECK(!delayed_work_pending(&c.dw));
	(void) schedule_delayed_work(&c.dw, 0);
	(void) cancel_delayed_work_sync(&c.dw);
	CHECK(!delayed_work_pending(&c.dw));
}

static atomic_t ord_seq;
static int ord_bad;

struct ord_item {
	struct work_struct	w;
	int			idx;
};

static void
ord_fn(struct work_struct *w)
{
	struct ord_item *it = container_of(w, struct ord_item, w);

	if (atomic_inc_return(&ord_seq) != it->idx + 1)
		ord_bad++;
}

static void
self_free_fn(struct work_struct *w)
{
	struct ord_item *it = container_of(w, struct ord_item, w);

	atomic_inc(&ord_seq);
	kfree(it);
}

TEST(ordered_and_flush)
{
	struct workqueue_struct *wq;
	struct ord_item items[32];
	struct ord_item *dyn;
	int i;

	wq = alloc_ordered_workqueue("lkpi_test_%d", 0, 1);
	CHECK(wq != NULL);
	if (wq == NULL)
		return;
	atomic_set(&ord_seq, 0);
	ord_bad = 0;
	for (i = 0; i < 32; i++) {
		INIT_WORK(&items[i].w, ord_fn);
		items[i].idx = i;
		CHECK(queue_work(wq, &items[i].w));
	}
	flush_workqueue(wq);
	CHECK_EQ(atomic_read(&ord_seq), 32);
	CHECK_EQ(ord_bad, 0);

	/* Work functions may free their own item. */
	for (i = 0; i < 8; i++) {
		dyn = kzalloc(sizeof (*dyn), GFP_KERNEL);
		INIT_WORK(&dyn->w, self_free_fn);
		(void) queue_work(wq, &dyn->w);
	}
	drain_workqueue(wq);
	CHECK_EQ(atomic_read(&ord_seq), 40);
	destroy_workqueue(wq);

	wq = alloc_workqueue("lkpi_multi", WQ_UNBOUND, 4);
	CHECK(wq != NULL);
	if (wq != NULL) {
		atomic_set(&ord_seq, 0);
		for (i = 0; i < 8; i++) {
			dyn = kzalloc(sizeof (*dyn), GFP_KERNEL);
			INIT_WORK(&dyn->w, self_free_fn);
			(void) queue_work(wq, &dyn->w);
		}
		destroy_workqueue(wq);	/* drains first */
		CHECK_EQ(atomic_read(&ord_seq), 8);
	}
}

/* A work item that waits for another item on the same workqueue. */
static struct work_struct dep_a, dep_b;
static DECLARE_COMPLETION(dep_b_done);
static int dep_ok;

static void
dep_b_fn(struct work_struct *w)
{
	(void) w;
	complete(&dep_b_done);
}

static void
dep_a_fn(struct work_struct *w)
{
	(void) w;
	(void) queue_work(system_wq, &dep_b);
	dep_ok = wait_for_completion_timeout(&dep_b_done,
	    msecs_to_jiffies(2000)) != 0;
	CHECK(current_work() == &dep_a);
}

TEST(work_dependency)
{
	INIT_WORK(&dep_a, dep_a_fn);
	INIT_WORK(&dep_b, dep_b_fn);
	reinit_completion(&dep_b_done);
	dep_ok = 0;
	(void) queue_work(system_wq, &dep_a);
	flush_work(&dep_a);
	CHECK_EQ(dep_ok, 1);
	CHECK(current_work() == NULL);
}

struct rctx {
	struct rcu_work	rw;
	atomic_t	runs;
};

static void
rctx_fn(struct work_struct *w)
{
	struct rctx *c = container_of(to_rcu_work(w), struct rctx, rw);

	atomic_inc(&c->runs);
}

TEST(rcu_work)
{
	struct rctx c;

	memset(&c, 0, sizeof (c));
	INIT_RCU_WORK(&c.rw, rctx_fn);
	CHECK(queue_rcu_work(system_wq, &c.rw));
	CHECK(!queue_rcu_work(system_wq, &c.rw));
	CHECK(flush_rcu_work(&c.rw));
	CHECK_EQ(atomic_read(&c.runs), 1);
}

static atomic_t kt_loops;

static int
kt_fn(void *data)
{
	int *arg = data;

	while (!kthread_should_stop()) {
		if (kthread_should_park())
			kthread_parkme();
		atomic_inc(&kt_loops);
		set_current_state(TASK_INTERRUPTIBLE);
		if (!kthread_should_stop())
			schedule_timeout(msecs_to_jiffies(2));
		__set_current_state(TASK_RUNNING);
	}
	return (*arg + 1);
}

static int
kt_quick(void *data)
{
	complete(data);
	return (0);
}

static struct completion kt_done;

TEST(kthreads)
{
	struct task_struct *t;
	int arg = 41, n;

	atomic_set(&kt_loops, 0);
	t = kthread_create(kt_fn, &arg, "lkpi_kt%d", 7);
	CHECK(!IS_ERR(t));
	if (IS_ERR(t))
		return;
	CHECK_EQ(strcmp(t->comm, "lkpi_kt7"), 0);
	CHECK(kthread_data(t) == &arg);
	msleep(10);
	CHECK_EQ(atomic_read(&kt_loops), 0);	/* not started yet */
	(void) wake_up_process(t);
	msleep(20);
	CHECK(atomic_read(&kt_loops) > 0);
	CHECK_EQ(kthread_park(t), 0);
	n = atomic_read(&kt_loops);
	msleep(15);
	CHECK_EQ(atomic_read(&kt_loops), n);
	kthread_unpark(t);
	msleep(15);
	CHECK(atomic_read(&kt_loops) > n);
	CHECK_EQ(kthread_stop(t), 42);

	/* Stopped before it ever ran: the function is not called. */
	t = kthread_create(kt_fn, &arg, "lkpi_kt_never");
	CHECK(!IS_ERR(t));
	if (!IS_ERR(t))
		CHECK_EQ(kthread_stop(t), -EINTR);

	/* A thread that exits on its own. */
	init_completion(&kt_done);
	t = kthread_run(kt_quick, &kt_done, "lkpi_kt_quick");
	CHECK(!IS_ERR(t));
	wait_for_completion(&kt_done);
	CHECK(!kthread_should_stop());
}

static atomic_t kw_runs;

static void
kw_fn(struct kthread_work *kw)
{
	(void) kw;
	atomic_inc(&kw_runs);
}

TEST(kthread_worker)
{
	struct kthread_worker *w;
	struct kthread_work kw;
	struct kthread_delayed_work kdw;

	atomic_set(&kw_runs, 0);
	w = kthread_create_worker(0, "lkpi_kw");
	CHECK(!IS_ERR(w));
	if (IS_ERR(w))
		return;
	kthread_init_work(&kw, kw_fn);
	kthread_init_delayed_work(&kdw, kw_fn);
	CHECK(kthread_queue_work(w, &kw));
	kthread_flush_work(&kw);
	CHECK_EQ(atomic_read(&kw_runs), 1);
	CHECK(kthread_queue_delayed_work(w, &kdw, 1));
	(void) kthread_cancel_delayed_work_sync(&kdw);
	(void) kthread_mod_delayed_work(w, &kdw, 0);
	kthread_flush_worker(w);
	kthread_destroy_worker(w);
	CHECK(atomic_read(&kw_runs) >= 2);
}

LKPI_SUITE(workqueue)
{
	CHECK_EQ(lkpi_workqueue_init(), 0);
	RUN(timer_list_basic);
	RUN(hrtimer_basic);
	RUN(work_basic);
	RUN(delayed_work);
	RUN(ordered_and_flush);
	RUN(work_dependency);
	RUN(rcu_work);
	RUN(kthreads);
	RUN(kthread_worker);
	lkpi_workqueue_fini();
}
