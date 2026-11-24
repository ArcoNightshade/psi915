/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation from the documented behaviour of the Linux
 * completion API; function set cross-checked against FreeBSD's LinuxKPI
 * completion.h (BSD-2-Clause).
 *
 * A completion is a counter under a kmutex_t with a kcondvar_t
 * (src/linux_completion.c). Timeouts are in jiffies. The _interruptible
 * and _killable waits use cv_wait_sig(9F) and return -ERESTARTSYS when a
 * signal arrives (only possible in a user thread's context). complete()
 * may be called from interrupt handlers below LOCK_LEVEL only.
 */
#ifndef _LINUXKPI_LINUX_COMPLETION_H_
#define	_LINUXKPI_LINUX_COMPLETION_H_

#include <linuxkpi/platform.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/limits.h>

struct completion {
	unsigned int		done;
	lkpi_plat_mutex_t	lock;
	lkpi_plat_cv_t		cv;
};

#define	COMPLETION_INITIALIZER(work)					\
	{ .done = 0, .lock = LKPI_PLAT_MUTEX_INITIALIZER,		\
	    .cv = LKPI_PLAT_CV_INITIALIZER }
#define	COMPLETION_INITIALIZER_ONSTACK(work)				\
	(*({ init_completion(&(work)); &(work); }))
#define	DECLARE_COMPLETION(work)					\
	struct completion work = COMPLETION_INITIALIZER(work)
#define	DECLARE_COMPLETION_ONSTACK(work)	DECLARE_COMPLETION(work)

static inline void
init_completion(struct completion *c)
{
	c->done = 0;
	lkpi_plat_mutex_init(&c->lock, NULL);
	lkpi_plat_cv_init(&c->cv);
}

/* Not part of the Linux API (completions are never destroyed there). */
static inline void
lkpi_completion_destroy(struct completion *c)
{
	lkpi_plat_cv_destroy(&c->cv);
	lkpi_plat_mutex_destroy(&c->lock);
}

static inline void
reinit_completion(struct completion *c)
{
	WRITE_ONCE(c->done, 0);
}

void complete(struct completion *c);
void complete_all(struct completion *c);
void wait_for_completion(struct completion *c);
unsigned long wait_for_completion_timeout(struct completion *c,
    unsigned long timeout);
int wait_for_completion_interruptible(struct completion *c);
long wait_for_completion_interruptible_timeout(struct completion *c,
    unsigned long timeout);
bool try_wait_for_completion(struct completion *c);
bool completion_done(struct completion *c);

#define	wait_for_completion_io(c)		wait_for_completion(c)
#define	wait_for_completion_io_timeout(c, t)	wait_for_completion_timeout(c, t)
#define	wait_for_completion_killable(c)		wait_for_completion_interruptible(c)
#define	wait_for_completion_killable_timeout(c, t)			\
	wait_for_completion_interruptible_timeout(c, t)
#define	wait_for_completion_state(c, state)				\
	wait_for_completion_interruptible(c)
#define	complete_on_current_cpu(c)		complete(c)
#define	complete_release(c)			complete(c)

#endif /* _LINUXKPI_LINUX_COMPLETION_H_ */
