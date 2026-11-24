/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation from the documented behaviour of the Linux
 * mutex API; function set cross-checked against FreeBSD's LinuxKPI
 * mutex.h (BSD-2-Clause).
 *
 * struct mutex is an adaptive illumos kmutex_t plus an owner field for
 * lockdep-lite (mutex_is_locked(), lockdep_assert_held()). Differences:
 *   - mutex_lock_interruptible()/_killable() cannot be interrupted and
 *     always return 0 (kmutex_t has no signal-aware enter);
 *   - taking a mutex the caller already holds warns before deadlocking.
 *
 * illumos <sys/mutex.h> declares mutex_init() and mutex_destroy() with
 * different signatures; the Linux names are macros over lkpi_mutex_*, so
 * illumos headers must be included before this one.
 */
#ifndef _LINUXKPI_LINUX_MUTEX_H_
#define	_LINUXKPI_LINUX_MUTEX_H_

#include <linuxkpi/platform.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/atomic.h>
#include <linux/refcount.h>
#include <linux/bug.h>

struct mutex {
	lkpi_plat_mutex_t	m;
	void			*owner;
};

#define	__MUTEX_INITIALIZER(name)					\
	{ .m = LKPI_PLAT_MUTEX_INITIALIZER, .owner = NULL }
#define	DEFINE_MUTEX(name)	struct mutex name = __MUTEX_INITIALIZER(name)

void lkpi_lock_recursion(const void *lock, const char *what);

static inline void
lkpi_mutex_init(struct mutex *m)
{
	lkpi_plat_mutex_init(&m->m, NULL);
	m->owner = NULL;
}

static inline void
lkpi_mutex_destroy(struct mutex *m)
{
	WARN_ON(READ_ONCE(m->owner) != NULL);
	lkpi_plat_mutex_destroy(&m->m);
}

#define	mutex_init(m)		lkpi_mutex_init(m)
#define	__mutex_init(m, name, key) lkpi_mutex_init(m)
#define	mutex_destroy(m)	lkpi_mutex_destroy(m)

static inline void
mutex_lock(struct mutex *m)
{
	void *self = lkpi_plat_curthread();

	if (unlikely(READ_ONCE(m->owner) == self))
		lkpi_lock_recursion(m, "mutex");
	lkpi_plat_mutex_enter(&m->m);
	WRITE_ONCE(m->owner, self);
}

/* Returns 1 if the mutex was taken. */
static inline int
mutex_trylock(struct mutex *m)
{
	if (!lkpi_plat_mutex_tryenter(&m->m))
		return (0);
	WRITE_ONCE(m->owner, lkpi_plat_curthread());
	return (1);
}

static inline void
mutex_unlock(struct mutex *m)
{
	WARN_ON_ONCE(READ_ONCE(m->owner) != lkpi_plat_curthread());
	WRITE_ONCE(m->owner, NULL);
	lkpi_plat_mutex_exit(&m->m);
}

static inline bool
mutex_is_locked(struct mutex *m)
{
	return (READ_ONCE(m->owner) != NULL);
}

static inline bool
lkpi_mutex_held(const struct mutex *m)
{
	return (READ_ONCE(m->owner) == lkpi_plat_curthread());
}

static inline int __must_check
mutex_lock_interruptible(struct mutex *m)
{
	mutex_lock(m);
	return (0);
}

#define	mutex_lock_killable(m)			mutex_lock_interruptible(m)
#define	mutex_lock_nested(m, subclass)		mutex_lock(m)
#define	mutex_lock_interruptible_nested(m, s)	mutex_lock_interruptible(m)
#define	mutex_lock_killable_nested(m, s)	mutex_lock_interruptible(m)
#define	mutex_lock_nest_lock(m, nest)		mutex_lock(m)
#define	mutex_lock_io(m)			mutex_lock(m)
#define	mutex_lock_io_nested(m, s)		mutex_lock(m)
#define	mutex_trylock_nested(m, s)		mutex_trylock(m)

/*
 * Decrement; if the result is zero, return true with the mutex held.
 */
static inline bool
atomic_dec_and_mutex_lock(atomic_t *a, struct mutex *m)
{
	if (atomic_add_unless(a, -1, 1))
		return (false);
	mutex_lock(m);
	if (atomic_dec_return(a) == 0)
		return (true);
	mutex_unlock(m);
	return (false);
}

static inline bool
refcount_dec_and_mutex_lock(refcount_t *r, struct mutex *m)
{
	if (refcount_dec_not_one(r))
		return (false);
	mutex_lock(m);
	if (!refcount_dec_and_test(r)) {
		mutex_unlock(m);
		return (false);
	}
	return (true);
}

#include <linux/lockdep.h>

#endif /* _LINUXKPI_LINUX_MUTEX_H_ */
