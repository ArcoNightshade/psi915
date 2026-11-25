/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * Wound/wait mutexes and semaphores (see <linux/ww_mutex.h>,
 * <linux/semaphore.h>). Clean-room.
 *
 * A ww_mutex is held when base.owner != NULL. ww->ctx is the holder's
 * acquire context (NULL for a context-less lock). A context that waits
 * records the mutex in lkpi_waiting_on, so a wound can wake it.
 */

#include <linux/kernel.h>
#include <linux/ww_mutex.h>
#include <linux/semaphore.h>
#include <linux/sched.h>
#include <linux/jiffies.h>
#include <linux/errno.h>
#include <linux/bug.h>

static lkpi_plat_mutex_t lkpi_ww_lock = LKPI_PLAT_MUTEX_INITIALIZER;

void
ww_acquire_init(struct ww_acquire_ctx *ctx, struct ww_class *class)
{
	ctx->task = current;
	ctx->stamp = (unsigned long)atomic_long_inc_return(&class->stamp);
	ctx->acquired = 0;
	ctx->wounded = false;
	ctx->is_wait_die = class->is_wait_die;
	ctx->ww_class = class;
	ctx->lkpi_waiting_on = NULL;
}

void
ww_acquire_fini(struct ww_acquire_ctx *ctx)
{
	WARN_ON_ONCE(ctx->acquired != 0);
}

/* a is younger than b (started later). */
static inline bool
lkpi_ww_younger(const struct ww_acquire_ctx *a, const struct ww_acquire_ctx *b)
{
	return ((long)(a->stamp - b->stamp) > 0);
}

static int
lkpi_ww_lock_common(struct ww_mutex *ww, struct ww_acquire_ctx *ctx,
    bool intr, bool slow)
{
	void *self = lkpi_plat_curthread();
	struct ww_acquire_ctx *holder;
	int err = 0;

	lkpi_plat_mutex_enter(&lkpi_ww_lock);
	if (ctx != NULL) {
		if (ww->ctx == ctx) {
			lkpi_plat_mutex_exit(&lkpi_ww_lock);
			return (-EALREADY);
		}
		if (ctx->acquired == 0)
			ctx->wounded = false;
	} else if (ww->base.owner == self) {
		lkpi_lock_recursion(ww, "ww_mutex");
	}
	for (;;) {
		if (ww->base.owner == NULL) {
			ww->base.owner = self;
			ww->ctx = ctx;
			if (ctx != NULL)
				ctx->acquired++;
			break;
		}
		holder = ww->ctx;
		if (ctx != NULL && !slow && ctx->acquired > 0) {
			if (ctx->is_wait_die) {
				if (holder != NULL &&
				    lkpi_ww_younger(ctx, holder)) {
					err = -EDEADLK;
					break;
				}
			} else if (ctx->wounded) {
				err = -EDEADLK;
				break;
			}
		}
		if (ctx != NULL && !ctx->is_wait_die && holder != NULL &&
		    lkpi_ww_younger(holder, ctx) && !holder->wounded) {
			/* Wound the younger holder; wake it if it waits. */
			holder->wounded = true;
			if (holder->lkpi_waiting_on != NULL)
				lkpi_plat_cv_broadcast(
				    &holder->lkpi_waiting_on->lkpi_cv);
		}
		if (intr && lkpi_plat_signal_pending()) {
			err = -EINTR;
			break;
		}
		if (ctx != NULL)
			ctx->lkpi_waiting_on = ww;
		if (intr)
			(void) lkpi_plat_cv_wait_sig(&ww->lkpi_cv,
			    &lkpi_ww_lock);
		else
			lkpi_plat_cv_wait(&ww->lkpi_cv, &lkpi_ww_lock);
		if (ctx != NULL)
			ctx->lkpi_waiting_on = NULL;
	}
	lkpi_plat_mutex_exit(&lkpi_ww_lock);
	return (err);
}

int
ww_mutex_lock(struct ww_mutex *ww, struct ww_acquire_ctx *ctx)
{
	return (lkpi_ww_lock_common(ww, ctx, false, false));
}

int
ww_mutex_lock_interruptible(struct ww_mutex *ww, struct ww_acquire_ctx *ctx)
{
	return (lkpi_ww_lock_common(ww, ctx, true, false));
}

void
ww_mutex_lock_slow(struct ww_mutex *ww, struct ww_acquire_ctx *ctx)
{
	WARN_ON_ONCE(ctx != NULL && ctx->acquired != 0);
	(void) lkpi_ww_lock_common(ww, ctx, false, true);
}

int
ww_mutex_lock_slow_interruptible(struct ww_mutex *ww,
    struct ww_acquire_ctx *ctx)
{
	WARN_ON_ONCE(ctx != NULL && ctx->acquired != 0);
	return (lkpi_ww_lock_common(ww, ctx, true, true));
}

int
ww_mutex_trylock(struct ww_mutex *ww, struct ww_acquire_ctx *ctx)
{
	int ok = 0;

	lkpi_plat_mutex_enter(&lkpi_ww_lock);
	if (ww->base.owner == NULL) {
		ww->base.owner = lkpi_plat_curthread();
		ww->ctx = ctx;
		if (ctx != NULL)
			ctx->acquired++;
		ok = 1;
	}
	lkpi_plat_mutex_exit(&lkpi_ww_lock);
	return (ok);
}

void
ww_mutex_unlock(struct ww_mutex *ww)
{
	lkpi_plat_mutex_enter(&lkpi_ww_lock);
	WARN_ON_ONCE(ww->base.owner == NULL);
	if (ww->ctx != NULL) {
		WARN_ON_ONCE(ww->ctx->acquired == 0);
		ww->ctx->acquired--;
	}
	ww->ctx = NULL;
	ww->base.owner = NULL;
	lkpi_plat_cv_broadcast(&ww->lkpi_cv);
	lkpi_plat_mutex_exit(&lkpi_ww_lock);
}

/* Semaphores. */

static int
lkpi_down_common(struct semaphore *sem, long timeout, bool intr)
{
	bool forever = timeout == MAX_SCHEDULE_TIMEOUT;
	unsigned long end = lkpi_plat_ticks() + (unsigned long)timeout;
	long left;
	int r, err = 0;

	lkpi_plat_mutex_enter(&sem->lock);
	while (sem->count == 0) {
		if (intr && lkpi_plat_signal_pending()) {
			err = -EINTR;
			break;
		}
		if (forever) {
			if (intr)
				(void) lkpi_plat_cv_wait_sig(&sem->cv,
				    &sem->lock);
			else
				lkpi_plat_cv_wait(&sem->cv, &sem->lock);
			continue;
		}
		left = (long)(end - lkpi_plat_ticks());
		if (left <= 0) {
			err = -ETIME;
			break;
		}
		r = lkpi_plat_cv_timedwait(&sem->cv, &sem->lock, left, intr);
		(void) r;
	}
	if (err == 0)
		sem->count--;
	lkpi_plat_mutex_exit(&sem->lock);
	return (err);
}

void
down(struct semaphore *sem)
{
	(void) lkpi_down_common(sem, MAX_SCHEDULE_TIMEOUT, false);
}

int
down_interruptible(struct semaphore *sem)
{
	return (lkpi_down_common(sem, MAX_SCHEDULE_TIMEOUT, true));
}

int
down_timeout(struct semaphore *sem, long timeout)
{
	return (lkpi_down_common(sem, timeout < 0 ? 0 : timeout, false));
}

/* 0 if acquired, 1 if not (Linux convention). */
int
down_trylock(struct semaphore *sem)
{
	int r = 1;

	lkpi_plat_mutex_enter(&sem->lock);
	if (sem->count > 0) {
		sem->count--;
		r = 0;
	}
	lkpi_plat_mutex_exit(&sem->lock);
	return (r);
}

void
up(struct semaphore *sem)
{
	lkpi_plat_mutex_enter(&sem->lock);
	sem->count++;
	lkpi_plat_cv_signal(&sem->cv);
	lkpi_plat_mutex_exit(&sem->lock);
}
