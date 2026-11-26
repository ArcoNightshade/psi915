/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * Timer engine for timer_list and hrtimer (see <linuxkpi/timer_engine.h>,
 * <linux/timer.h>, <linux/hrtimer.h>). Clean-room design.
 *
 * All state is protected by lkpi_timer_lock. The service thread waits on
 * lkpi_timer_cv until the earliest deadline; arming a timer that becomes
 * the earliest signals it. lkpi_timer_cur is the node whose callback is
 * running (NULL when idle); it is only compared, never dereferenced after
 * the callback returned, because a callback may free its own timer.
 * Cancellers waiting for a running callback sleep on lkpi_timer_done_cv.
 *
 * Why not timeout(9F): untimeout() waits for a running callout, so a
 * cancel under a lock the callback takes deadlocks, and a callout cannot
 * tell a stale firing from a re-armed one without allocation. A private
 * thread gives exact Linux semantics and is the same code in userland
 * tests and the kernel.
 */

#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/hrtimer.h>
#include <linux/rbtree.h>
#include <linux/bug.h>
#include <linuxkpi/subsys.h>
#include <linuxkpi/timer_engine.h>

static lkpi_plat_mutex_t lkpi_timer_lock = LKPI_PLAT_MUTEX_INITIALIZER;
static lkpi_plat_cv_t lkpi_timer_cv = LKPI_PLAT_CV_INITIALIZER;
static lkpi_plat_cv_t lkpi_timer_done_cv = LKPI_PLAT_CV_INITIALIZER;
static struct rb_root_cached lkpi_timer_tree = RB_ROOT_CACHED;
static struct lkpi_timer_node *lkpi_timer_cur;
static void *lkpi_timer_self;
static struct lkpi_plat_thread *lkpi_timer_thread;
static bool lkpi_timer_stop;

static bool
lkpi_timer_less(struct rb_node *a, const struct rb_node *b)
{
	return (rb_entry(a, struct lkpi_timer_node, rb)->deadline <
	    rb_entry(b, struct lkpi_timer_node, rb)->deadline);
}

static void
lkpi_timer_worker(void *arg)
{
	struct lkpi_timer_node *n;
	struct rb_node *first;
	u64 now, next = 0;
	bool rearm;

	(void) arg;
	lkpi_plat_mutex_enter(&lkpi_timer_lock);
	lkpi_timer_self = lkpi_plat_curthread();
	while (!lkpi_timer_stop) {
		first = rb_first_cached(&lkpi_timer_tree);
		if (first == NULL) {
			lkpi_plat_cv_wait(&lkpi_timer_cv, &lkpi_timer_lock);
			continue;
		}
		n = rb_entry(first, struct lkpi_timer_node, rb);
		now = lkpi_plat_nsec();
		if (n->deadline > now) {
			(void) lkpi_plat_cv_timedwait_ns(&lkpi_timer_cv,
			    &lkpi_timer_lock, n->deadline, false);
			continue;
		}
		(void) rb_erase_cached(&n->rb, &lkpi_timer_tree);
		RB_CLEAR_NODE(&n->rb);
		n->queued = false;
		lkpi_timer_cur = n;
		lkpi_plat_mutex_exit(&lkpi_timer_lock);

		rearm = n->run(n, &next);

		lkpi_plat_mutex_enter(&lkpi_timer_lock);
		/* rearm: the callback asked to restart, so n is still live. */
		if (rearm && !n->queued && !n->shutdown) {
			n->deadline = next;
			n->queued = true;
			(void) rb_add_cached(&n->rb, &lkpi_timer_tree,
			    lkpi_timer_less);
		}
		lkpi_timer_cur = NULL;
		lkpi_plat_cv_broadcast(&lkpi_timer_done_cv);
	}
	lkpi_timer_self = NULL;
	lkpi_plat_mutex_exit(&lkpi_timer_lock);
}

/* Caller holds lkpi_timer_lock. */
static int
lkpi_timer_start_locked(void)
{
	if (lkpi_timer_thread != NULL)
		return (0);
	lkpi_timer_stop = false;
	lkpi_timer_thread = lkpi_plat_thread_create(lkpi_timer_worker, NULL,
	    "linuxkpi_timer");
	return (lkpi_timer_thread != NULL ? 0 : ENOMEM);
}

int
lkpi_timer_init(void)
{
	int error;

	lkpi_plat_mutex_enter(&lkpi_timer_lock);
	error = lkpi_timer_start_locked();
	lkpi_plat_mutex_exit(&lkpi_timer_lock);
	return (error);
}

void
lkpi_timer_fini(void)
{
	struct lkpi_plat_thread *t;
	struct rb_node *rb;

	lkpi_plat_mutex_enter(&lkpi_timer_lock);
	t = lkpi_timer_thread;
	lkpi_timer_thread = NULL;
	lkpi_timer_stop = true;
	lkpi_plat_cv_broadcast(&lkpi_timer_cv);
	lkpi_plat_mutex_exit(&lkpi_timer_lock);
	if (t != NULL)
		lkpi_plat_thread_join(t);
	lkpi_plat_mutex_enter(&lkpi_timer_lock);
	while ((rb = rb_first_cached(&lkpi_timer_tree)) != NULL) {
		struct lkpi_timer_node *n =
		    rb_entry(rb, struct lkpi_timer_node, rb);

		WARN_ONCE(1, "linuxkpi: timer %p still armed at unload\n", n);
		(void) rb_erase_cached(rb, &lkpi_timer_tree);
		RB_CLEAR_NODE(rb);
		n->queued = false;
	}
	lkpi_plat_mutex_exit(&lkpi_timer_lock);
}

/* Caller holds lkpi_timer_lock. */
static bool
lkpi_timer_dequeue_locked(struct lkpi_timer_node *n)
{
	if (!n->queued)
		return (false);
	(void) rb_erase_cached(&n->rb, &lkpi_timer_tree);
	RB_CLEAR_NODE(&n->rb);
	n->queued = false;
	return (true);
}

bool
lkpi_timer_arm(struct lkpi_timer_node *n, u64 deadline)
{
	bool was;

	lkpi_plat_mutex_enter(&lkpi_timer_lock);
	if (n->shutdown) {
		lkpi_plat_mutex_exit(&lkpi_timer_lock);
		return (false);
	}
	if (unlikely(lkpi_timer_thread == NULL))
		(void) lkpi_timer_start_locked();	/* userland */
	was = lkpi_timer_dequeue_locked(n);
	n->deadline = deadline;
	n->queued = true;
	if (rb_add_cached(&n->rb, &lkpi_timer_tree, lkpi_timer_less) != NULL)
		lkpi_plat_cv_signal(&lkpi_timer_cv);	/* new earliest */
	lkpi_plat_mutex_exit(&lkpi_timer_lock);
	return (was);
}

bool
lkpi_timer_disarm(struct lkpi_timer_node *n)
{
	bool was;

	lkpi_plat_mutex_enter(&lkpi_timer_lock);
	was = lkpi_timer_dequeue_locked(n);
	lkpi_plat_mutex_exit(&lkpi_timer_lock);
	return (was);
}

bool
lkpi_timer_disarm_sync(struct lkpi_timer_node *n, bool shutdown)
{
	bool was;

	lkpi_plat_mutex_enter(&lkpi_timer_lock);
	if (shutdown)
		n->shutdown = true;
	was = lkpi_timer_dequeue_locked(n);
	while (lkpi_timer_cur == n &&
	    lkpi_plat_curthread() != lkpi_timer_self) {
		lkpi_plat_cv_wait(&lkpi_timer_done_cv, &lkpi_timer_lock);
		/* The callback may have re-armed the timer. */
		(void) lkpi_timer_dequeue_locked(n);
	}
	lkpi_plat_mutex_exit(&lkpi_timer_lock);
	return (was);
}

int
lkpi_timer_try_disarm(struct lkpi_timer_node *n)
{
	int r;

	lkpi_plat_mutex_enter(&lkpi_timer_lock);
	if (lkpi_timer_cur == n)
		r = -1;
	else
		r = lkpi_timer_dequeue_locked(n) ? 1 : 0;
	lkpi_plat_mutex_exit(&lkpi_timer_lock);
	return (r);
}

bool
lkpi_timer_running(const struct lkpi_timer_node *n)
{
	return (READ_ONCE(lkpi_timer_cur) == n);
}

u64
lkpi_timer_jiffies_to_deadline(unsigned long expires)
{
	long delta = (long)(expires - lkpi_plat_ticks());
	u64 now = lkpi_plat_nsec();

	if (delta <= 0)
		return (now);
	return (now + (u64)delta * (1000000000ULL / lkpi_plat_hz()));
}

/* timer_list */

bool
lkpi_timer_list_run(struct lkpi_timer_node *n, u64 *deadline)
{
	struct timer_list *t = container_of(n, struct timer_list, lkpi_node);
	void (*fn)(struct timer_list *) = t->function;

	(void) deadline;
	if (fn != NULL)
		fn(t);
	return (false);
}

int
mod_timer(struct timer_list *t, unsigned long expires)
{
	if (t->function == NULL)
		return (0);	/* shut down: silently ignored, as in Linux */
	t->expires = expires;
	return (lkpi_timer_arm(&t->lkpi_node,
	    lkpi_timer_jiffies_to_deadline(expires)) ? 1 : 0);
}

int
mod_timer_pending(struct timer_list *t, unsigned long expires)
{
	if (!timer_pending(t))
		return (0);
	return (mod_timer(t, expires));
}

/* Arm, or move an armed timer earlier (never later). */
int
timer_reduce(struct timer_list *t, unsigned long expires)
{
	if (timer_pending(t) && time_before_eq(t->expires, expires))
		return (1);
	return (mod_timer(t, expires));
}

void
add_timer(struct timer_list *t)
{
	WARN_ON_ONCE(timer_pending(t));
	(void) mod_timer(t, t->expires);
}

int
del_timer(struct timer_list *t)
{
	return (lkpi_timer_disarm(&t->lkpi_node) ? 1 : 0);
}

int
del_timer_sync(struct timer_list *t)
{
	return (lkpi_timer_disarm_sync(&t->lkpi_node, false) ? 1 : 0);
}

int
try_to_del_timer_sync(struct timer_list *t)
{
	return (lkpi_timer_try_disarm(&t->lkpi_node));
}

int
timer_shutdown_sync(struct timer_list *t)
{
	int r = lkpi_timer_disarm_sync(&t->lkpi_node, true) ? 1 : 0;

	t->function = NULL;
	return (r);
}

int
timer_shutdown(struct timer_list *t)
{
	int r;

	lkpi_plat_mutex_enter(&lkpi_timer_lock);
	t->lkpi_node.shutdown = true;
	r = lkpi_timer_dequeue_locked(&t->lkpi_node) ? 1 : 0;
	lkpi_plat_mutex_exit(&lkpi_timer_lock);
	return (r);
}

/* hrtimer */

static u64
lkpi_hrtimer_deadline(const struct hrtimer *t)
{
	ktime_t now = ktime_get(), rel;

	if (t->lkpi_clock == CLOCK_REALTIME || t->lkpi_clock == CLOCK_TAI) {
		rel = t->lkpi_expires - ktime_get_real();
		return ((u64)(rel > 0 ? now + rel : now));
	}
	return ((u64)(t->lkpi_expires > 0 ? t->lkpi_expires : 0));
}

bool
lkpi_hrtimer_run(struct lkpi_timer_node *n, u64 *deadline)
{
	struct hrtimer *t = container_of(n, struct hrtimer, lkpi_node);

	if (t->function == NULL || t->function(t) != HRTIMER_RESTART)
		return (false);
	/* Re-armed by the engine at the (forwarded) expiry time. */
	*deadline = lkpi_hrtimer_deadline(t);
	return (true);
}

void
hrtimer_start_range_ns(struct hrtimer *t, ktime_t tim, u64 slack,
    enum hrtimer_mode mode)
{
	(void) slack;
	if (mode & HRTIMER_MODE_REL)
		tim = ktime_add(lkpi_hrtimer_clock_now(t), tim);
	t->lkpi_expires = tim;
	(void) lkpi_timer_arm(&t->lkpi_node, lkpi_hrtimer_deadline(t));
}

int
hrtimer_cancel(struct hrtimer *t)
{
	return (lkpi_timer_disarm_sync(&t->lkpi_node, false) ? 1 : 0);
}

int
hrtimer_try_to_cancel(struct hrtimer *t)
{
	return (lkpi_timer_try_disarm(&t->lkpi_node));
}

/*
 * Move the expiry forward by whole intervals until it is after now;
 * returns the number of intervals added (0 if already in the future).
 */
u64
hrtimer_forward(struct hrtimer *t, ktime_t now, ktime_t interval)
{
	ktime_t delta = now - t->lkpi_expires;
	u64 n;

	if (delta < 0)
		return (0);
	if (interval <= 0)
		interval = 1;
	n = (u64)(delta / interval) + 1;
	t->lkpi_expires += (ktime_t)n * interval;
	return (n);
}
