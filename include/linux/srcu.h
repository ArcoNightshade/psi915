/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation of Sleepable RCU (src/linux_srcu.c).
 *
 * Each srcu_struct has two reader counters and an index. A reader
 * increments ctr[idx & 1], issues a full barrier and re-reads idx; if the
 * index changed meanwhile it backs out and retries, so a reader is
 * counted on the index current at the instant its section starts.
 * synchronize_srcu() flips idx (full barrier) and sleeps until the old
 * counter drains. Readers may sleep, take any lock, and pass the index to
 * srcu_read_unlock() on another thread. Independent srcu_structs do not
 * wait for each other. call_srcu() is not provided (no consumer in the
 * Gen9 DRM/i915 set); srcu_barrier() is therefore a no-op.
 */
#ifndef _LINUXKPI_LINUX_SRCU_H_
#define	_LINUXKPI_LINUX_SRCU_H_

#include <linuxkpi/platform.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/atomic.h>
#include <linux/mutex.h>
#include <linux/rcupdate.h>

struct srcu_struct {
	atomic_long_t	ctr[2];
	unsigned long	idx;
	struct mutex	lock;	/* serialises updaters */
};

#define	__SRCU_STRUCT_INIT(name)					\
	{ .ctr = { ATOMIC_LONG_INIT(0), ATOMIC_LONG_INIT(0) }, .idx = 0, \
	    .lock = __MUTEX_INITIALIZER(name.lock) }
#define	DEFINE_SRCU(name)	struct srcu_struct name = __SRCU_STRUCT_INIT(name)
#define	DEFINE_STATIC_SRCU(name)					\
	static struct srcu_struct name = __SRCU_STRUCT_INIT(name)

int init_srcu_struct(struct srcu_struct *ssp);
void cleanup_srcu_struct(struct srcu_struct *ssp);
int srcu_read_lock(struct srcu_struct *ssp);
void srcu_read_unlock(struct srcu_struct *ssp, int idx);
void synchronize_srcu(struct srcu_struct *ssp);
bool srcu_read_lock_held(const struct srcu_struct *ssp);

#define	synchronize_srcu_expedited(ssp)	synchronize_srcu(ssp)
#define	srcu_barrier(ssp)		do { (void)(ssp); } while (0)
#define	srcu_read_lock_fast(ssp)	srcu_read_lock(ssp)
#define	srcu_read_unlock_fast(ssp, i)	srcu_read_unlock(ssp, i)
#define	srcu_down_read(ssp)		srcu_read_lock(ssp)
#define	srcu_up_read(ssp, i)		srcu_read_unlock(ssp, i)
#define	srcu_dereference(p, ssp)	rcu_dereference_raw(p)
#define	srcu_dereference_check(p, ssp, c) rcu_dereference_raw(p)

#endif /* _LINUXKPI_LINUX_SRCU_H_ */
