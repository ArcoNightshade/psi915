/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation of Linux counting semaphores
 * (src/linux_ww_mutex.c): a count under a kmutex_t with a condition
 * variable. down_interruptible() returns -EINTR on a signal,
 * down_timeout() -ETIME on timeout (jiffies), down_trylock() 0 when it
 * took the semaphore. up() may be called from interrupt handlers below
 * LOCK_LEVEL.
 */
#ifndef _LINUXKPI_LINUX_SEMAPHORE_H_
#define	_LINUXKPI_LINUX_SEMAPHORE_H_

#include <linuxkpi/platform.h>
#include <linux/compiler.h>
#include <linux/types.h>

struct semaphore {
	lkpi_plat_mutex_t	lock;
	lkpi_plat_cv_t		cv;
	unsigned int		count;
};

#define	__SEMAPHORE_INITIALIZER(name, n)				\
	{ .lock = LKPI_PLAT_MUTEX_INITIALIZER,				\
	    .cv = LKPI_PLAT_CV_INITIALIZER, .count = (n) }
#define	DEFINE_SEMAPHORE(name, n)					\
	struct semaphore name = __SEMAPHORE_INITIALIZER(name, n)

static inline void
sema_init(struct semaphore *sem, int val)
{
	lkpi_plat_mutex_init(&sem->lock, NULL);
	lkpi_plat_cv_init(&sem->cv);
	sem->count = (unsigned int)val;
}

/* Not part of the Linux API. */
static inline void
lkpi_sema_destroy(struct semaphore *sem)
{
	lkpi_plat_cv_destroy(&sem->cv);
	lkpi_plat_mutex_destroy(&sem->lock);
}

void down(struct semaphore *sem);
int __must_check down_interruptible(struct semaphore *sem);
int __must_check down_timeout(struct semaphore *sem, long timeout);
int __must_check down_trylock(struct semaphore *sem);
void up(struct semaphore *sem);
#define	down_killable(sem)	down_interruptible(sem)

#endif /* _LINUXKPI_LINUX_SEMAPHORE_H_ */
