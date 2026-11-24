/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation from the documented behaviour of the Linux
 * rw_semaphore API; cross-checked against FreeBSD's LinuxKPI rwsem.h
 * (BSD-2-Clause).
 *
 * struct rw_semaphore is a krwlock_t plus the writing owner and a reader
 * count for lockdep-lite. The _interruptible/_killable variants cannot be
 * interrupted and return 0. A reader that re-acquires behind a waiting
 * writer deadlocks (as in Linux). up_write() must be called by the
 * thread that called down_write().
 */
#ifndef _LINUXKPI_LINUX_RWSEM_H_
#define	_LINUXKPI_LINUX_RWSEM_H_

#include <linuxkpi/platform.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/atomic.h>
#include <linux/bug.h>

struct rw_semaphore {
	lkpi_plat_rwlock_t	rw;
	void			*owner;
	atomic_t		readers;
};

#define	__RWSEM_INITIALIZER(name)					\
	{ .rw = LKPI_PLAT_RWLOCK_INITIALIZER, .owner = NULL,		\
	    .readers = ATOMIC_INIT(0) }
#define	DECLARE_RWSEM(name)	struct rw_semaphore name = __RWSEM_INITIALIZER(name)

void lkpi_lock_recursion(const void *lock, const char *what);

static inline void
lkpi_init_rwsem(struct rw_semaphore *s)
{
	lkpi_plat_rwlock_init(&s->rw);
	s->owner = NULL;
	atomic_set(&s->readers, 0);
}

#define	init_rwsem(s)		lkpi_init_rwsem(s)

/* Not part of the Linux API (rwsems are never destroyed there). */
static inline void
lkpi_rwsem_destroy(struct rw_semaphore *s)
{
	lkpi_plat_rwlock_destroy(&s->rw);
}

static inline void
down_read(struct rw_semaphore *s)
{
	lkpi_plat_rwlock_enter(&s->rw, false);
	atomic_inc(&s->readers);
}

static inline int
down_read_trylock(struct rw_semaphore *s)
{
	if (!lkpi_plat_rwlock_tryenter(&s->rw, false))
		return (0);
	atomic_inc(&s->readers);
	return (1);
}

static inline void
up_read(struct rw_semaphore *s)
{
	atomic_dec(&s->readers);
	lkpi_plat_rwlock_exit(&s->rw);
}

static inline void
down_write(struct rw_semaphore *s)
{
	void *self = lkpi_plat_curthread();

	if (unlikely(READ_ONCE(s->owner) == self))
		lkpi_lock_recursion(s, "rw_semaphore");
	lkpi_plat_rwlock_enter(&s->rw, true);
	WRITE_ONCE(s->owner, self);
}

static inline int
down_write_trylock(struct rw_semaphore *s)
{
	if (!lkpi_plat_rwlock_tryenter(&s->rw, true))
		return (0);
	WRITE_ONCE(s->owner, lkpi_plat_curthread());
	return (1);
}

static inline void
up_write(struct rw_semaphore *s)
{
	WRITE_ONCE(s->owner, NULL);
	lkpi_plat_rwlock_exit(&s->rw);
}

static inline void
downgrade_write(struct rw_semaphore *s)
{
	atomic_inc(&s->readers);
	WRITE_ONCE(s->owner, NULL);
	lkpi_plat_rwlock_downgrade(&s->rw);
}

static inline int
rwsem_is_locked(struct rw_semaphore *s)
{
	return (READ_ONCE(s->owner) != NULL || atomic_read(&s->readers) > 0);
}

static inline bool
lkpi_rwsem_held(const struct rw_semaphore *s)
{
	return (READ_ONCE(s->owner) == lkpi_plat_curthread() ||
	    atomic_read(&s->readers) > 0);
}

static inline bool
lkpi_rwsem_write_held(const struct rw_semaphore *s)
{
	return (READ_ONCE(s->owner) == lkpi_plat_curthread());
}

static inline bool
lkpi_rwsem_read_held(const struct rw_semaphore *s)
{
	return (atomic_read(&s->readers) > 0);
}

static inline int __must_check
down_read_interruptible(struct rw_semaphore *s)
{
	down_read(s);
	return (0);
}

static inline int __must_check
down_write_killable(struct rw_semaphore *s)
{
	down_write(s);
	return (0);
}

#define	down_read_killable(s)		down_read_interruptible(s)
#define	down_read_nested(s, subclass)	down_read(s)
#define	down_read_non_owner(s)		down_read(s)
#define	up_read_non_owner(s)		up_read(s)
#define	down_write_nested(s, subclass)	down_write(s)
#define	down_write_nest_lock(s, nest)	down_write(s)
#define	down_write_killable_nested(s, subclass) down_write_killable(s)
#define	rwsem_is_contended(s)		(0)

#include <linux/lockdep.h>

#endif /* _LINUXKPI_LINUX_RWSEM_H_ */
