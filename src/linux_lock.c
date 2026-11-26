/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * Out-of-line parts of the lock and completion KPIs (see
 * <linux/spinlock.h>, <linux/mutex.h>, <linux/completion.h>). Clean-room.
 */

#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/rwsem.h>
#include <linux/completion.h>
#include <linux/jiffies.h>
#include <linux/errno.h>

/*
 * A thread is about to take a lock it already holds, which deadlocks for
 * every KPI lock type. Report it first, so the hang has an explanation.
 */
void
lkpi_lock_recursion(const void *lock, const char *what)
{
	WARN(1, "linuxkpi: recursive acquisition of %s %p (deadlock)\n",
	    what, lock);
}

void
complete(struct completion *c)
{
	lkpi_plat_mutex_enter(&c->lock);
	if (c->done != UINT_MAX)
		c->done++;
	lkpi_plat_cv_signal(&c->cv);
	lkpi_plat_mutex_exit(&c->lock);
}

void
complete_all(struct completion *c)
{
	lkpi_plat_mutex_enter(&c->lock);
	c->done = UINT_MAX;
	lkpi_plat_cv_broadcast(&c->cv);
	lkpi_plat_mutex_exit(&c->lock);
}

/*
 * Wait for c->done; consume one completion unless complete_all() was
 * used. timeout in jiffies, or MAX_SCHEDULE_TIMEOUT for none. Returns the
 * remaining time (at least 1) on success, 0 on timeout, -ERESTARTSYS if
 * interrupted.
 */
static long
lkpi_wait_for_common(struct completion *c, unsigned long timeout, bool intr)
{
	bool forever = timeout >= (unsigned long)MAX_SCHEDULE_TIMEOUT;
	unsigned long end = lkpi_plat_ticks() + timeout;
	long left = (long)timeout;
	int r;

	lkpi_plat_mutex_enter(&c->lock);
	while (c->done == 0) {
		if (forever) {
			if (!intr) {
				lkpi_plat_cv_wait(&c->cv, &c->lock);
				continue;
			}
			if (lkpi_plat_cv_wait_sig(&c->cv, &c->lock)) {
				lkpi_plat_mutex_exit(&c->lock);
				return (-ERESTARTSYS);
			}
			continue;
		}
		left = (long)(end - lkpi_plat_ticks());
		if (left <= 0) {
			lkpi_plat_mutex_exit(&c->lock);
			return (0);
		}
		r = lkpi_plat_cv_timedwait(&c->cv, &c->lock, left, intr);
		if (r == LKPI_PLAT_WAIT_INTR) {
			lkpi_plat_mutex_exit(&c->lock);
			return (-ERESTARTSYS);
		}
	}
	if (c->done != UINT_MAX)
		c->done--;
	lkpi_plat_mutex_exit(&c->lock);
	if (forever)
		return ((long)timeout);
	left = (long)(end - lkpi_plat_ticks());
	return (left > 0 ? left : 1);
}

void
wait_for_completion(struct completion *c)
{
	(void) lkpi_wait_for_common(c, MAX_SCHEDULE_TIMEOUT, false);
}

unsigned long
wait_for_completion_timeout(struct completion *c, unsigned long timeout)
{
	long r = lkpi_wait_for_common(c, timeout, false);

	return (r > 0 ? (unsigned long)r : 0);
}

int
wait_for_completion_interruptible(struct completion *c)
{
	long r = lkpi_wait_for_common(c, MAX_SCHEDULE_TIMEOUT, true);

	return (r < 0 ? (int)r : 0);
}

long
wait_for_completion_interruptible_timeout(struct completion *c,
    unsigned long timeout)
{
	return (lkpi_wait_for_common(c, timeout, true));
}

bool
try_wait_for_completion(struct completion *c)
{
	bool ok = false;

	lkpi_plat_mutex_enter(&c->lock);
	if (c->done != 0) {
		if (c->done != UINT_MAX)
			c->done--;
		ok = true;
	}
	lkpi_plat_mutex_exit(&c->lock);
	return (ok);
}

bool
completion_done(struct completion *c)
{
	return (READ_ONCE(c->done) != 0);
}

/*
 * Interruptible mutex acquisition (docs/12 L5). Waiters register in
 * m->lkpi_waiters and sleep on one of LKPI_MTX_WQ hashed condition
 * variables; mutex_unlock() broadcasts on the bucket when waiters exist.
 * The bucket lock closes the race between a failed tryenter and the
 * unlocker's broadcast. Killable waits sleep without signal wakeups in
 * short slices and test for a fatal signal, since an ordinary pending
 * signal would make a signal-aware sleep return at once.
 */
#define	LKPI_MTX_WQ	64

static struct {
	lkpi_plat_mutex_t	lock;
	lkpi_plat_cv_t		cv;
} lkpi_mtx_wq[LKPI_MTX_WQ] = {
	[0 ... LKPI_MTX_WQ - 1] = { LKPI_PLAT_MUTEX_INITIALIZER,
	    LKPI_PLAT_CV_INITIALIZER }
};

static unsigned int
lkpi_mtx_bucket(const struct mutex *m)
{
	uintptr_t a = (uintptr_t)m;

	return ((unsigned int)((a >> 4) ^ (a >> 12)) % LKPI_MTX_WQ);
}

void
lkpi_mutex_wake(struct mutex *m)
{
	unsigned int b = lkpi_mtx_bucket(m);

	lkpi_plat_mutex_enter(&lkpi_mtx_wq[b].lock);
	lkpi_plat_cv_broadcast(&lkpi_mtx_wq[b].cv);
	lkpi_plat_mutex_exit(&lkpi_mtx_wq[b].lock);
}

int
lkpi_mutex_lock_sig(struct mutex *m, bool killable_only)
{
	void *self = lkpi_plat_curthread();
	unsigned int b;
	int r = 0;

	if (unlikely(READ_ONCE(m->owner) == self))
		lkpi_lock_recursion(m, "mutex");
	if (lkpi_plat_mutex_tryenter(&m->m))
		goto got;
	b = lkpi_mtx_bucket(m);
	lkpi_plat_mutex_enter(&lkpi_mtx_wq[b].lock);
	__atomic_fetch_add(&m->lkpi_waiters, 1, __ATOMIC_SEQ_CST);
	for (;;) {
		if (lkpi_plat_mutex_tryenter(&m->m))
			break;
		if (killable_only) {
			if (lkpi_plat_fatal_signal_pending()) {
				r = -EINTR;
				break;
			}
			(void) lkpi_plat_cv_timedwait(&lkpi_mtx_wq[b].cv,
			    &lkpi_mtx_wq[b].lock, 1 + (long)lkpi_plat_hz() / 100,
			    false);
		} else {
			/*
			 * Timed as a fallback: code that drops m->m inside a
			 * condition-variable wait releases it without a wake.
			 */
			if (lkpi_plat_signal_pending() ||
			    lkpi_plat_cv_timedwait(&lkpi_mtx_wq[b].cv,
			    &lkpi_mtx_wq[b].lock, 1 + (long)lkpi_plat_hz() / 100,
			    true) == LKPI_PLAT_WAIT_INTR) {
				r = -EINTR;
				break;
			}
		}
	}
	__atomic_fetch_sub(&m->lkpi_waiters, 1, __ATOMIC_SEQ_CST);
	lkpi_plat_mutex_exit(&lkpi_mtx_wq[b].lock);
	if (r != 0)
		return (r);
got:
	WRITE_ONCE(m->owner, self);
	return (0);
}
