/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation of Linux wound/wait mutexes from their
 * documented behaviour (Documentation/locking/ww-mutex-design.rst).
 *
 * Both algorithms of the documentation are provided, selected by the
 * class: DEFINE_WD_CLASS (wait-die: a younger context that already holds
 * locks and finds the lock held by an older context gets -EDEADLK) and
 * DEFINE_WW_CLASS (wound-wait: an older context marks a younger holder
 * wounded and waits; the wounded context gets -EDEADLK at its next
 * contended acquisition while it holds locks, being woken if it is
 * already waiting). After -EDEADLK the caller drops all its locks and
 * takes the contended one with ww_mutex_lock_slow(), as in Linux.
 *
 * Implementation (src/linux_ww_mutex.c): all ww_mutex state is under one
 * global lock; each ww_mutex has a condition variable. `base` is a
 * struct mutex whose owner field is maintained, so lockdep_is_held(&
 * ww->base), mutex_is_locked(&ww->base) and lockdep_assert_held() work;
 * base's kmutex is never entered. ww_mutex waits are not usable from
 * interrupt context.
 */
#ifndef _LINUXKPI_LINUX_WW_MUTEX_H_
#define	_LINUXKPI_LINUX_WW_MUTEX_H_

#include <linuxkpi/platform.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/atomic.h>
#include <linux/mutex.h>
#include <linux/lockdep.h>

struct ww_class {
	atomic_long_t	stamp;
	bool		is_wait_die;
	const char	*acquire_name;
	const char	*mutex_name;
};

struct ww_acquire_ctx {
	struct task_struct	*task;
	unsigned long		stamp;
	unsigned int		acquired;
	bool			wounded;
	bool			is_wait_die;
	struct ww_class		*ww_class;
	struct ww_mutex		*lkpi_waiting_on;
};

struct ww_mutex {
	struct mutex		base;
	struct ww_acquire_ctx	*ctx;
	struct ww_class		*ww_class;
	lkpi_plat_cv_t		lkpi_cv;
};

#define	__WW_CLASS_INITIALIZER(name, wd)				\
	{ .stamp = ATOMIC_LONG_INIT(0), .is_wait_die = (wd),		\
	    .acquire_name = #name "_acquire", .mutex_name = #name "_mutex" }
#define	DEFINE_WW_CLASS(name)						\
	struct ww_class name = __WW_CLASS_INITIALIZER(name, false)
#define	DEFINE_WD_CLASS(name)						\
	struct ww_class name = __WW_CLASS_INITIALIZER(name, true)
#define	__WW_MUTEX_INITIALIZER(name, class)				\
	{ .base = __MUTEX_INITIALIZER(name.base), .ctx = NULL,		\
	    .ww_class = &(class), .lkpi_cv = LKPI_PLAT_CV_INITIALIZER }
#define	DEFINE_WW_MUTEX(name, class)					\
	struct ww_mutex name = __WW_MUTEX_INITIALIZER(name, class)

static inline void
ww_mutex_init(struct ww_mutex *ww, struct ww_class *class)
{
	lkpi_mutex_init(&ww->base);
	ww->ctx = NULL;
	ww->ww_class = class;
	lkpi_plat_cv_init(&ww->lkpi_cv);
}

static inline void
ww_mutex_destroy(struct ww_mutex *ww)
{
	lkpi_plat_cv_destroy(&ww->lkpi_cv);
	lkpi_mutex_destroy(&ww->base);
}

static inline bool
ww_mutex_is_locked(struct ww_mutex *ww)
{
	return (mutex_is_locked(&ww->base));
}

void ww_acquire_init(struct ww_acquire_ctx *ctx, struct ww_class *class);
void ww_acquire_fini(struct ww_acquire_ctx *ctx);

static inline void
ww_acquire_done(struct ww_acquire_ctx *ctx)
{
	(void) ctx;
}

int ww_mutex_lock(struct ww_mutex *ww, struct ww_acquire_ctx *ctx);
int __must_check ww_mutex_lock_interruptible(struct ww_mutex *ww,
    struct ww_acquire_ctx *ctx);
void ww_mutex_lock_slow(struct ww_mutex *ww, struct ww_acquire_ctx *ctx);
int __must_check ww_mutex_lock_slow_interruptible(struct ww_mutex *ww,
    struct ww_acquire_ctx *ctx);
int ww_mutex_trylock(struct ww_mutex *ww, struct ww_acquire_ctx *ctx);
void ww_mutex_unlock(struct ww_mutex *ww);

#endif /* _LINUXKPI_LINUX_WW_MUTEX_H_ */
