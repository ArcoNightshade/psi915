/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation from the documented behaviour of the Linux
 * lockdep annotation API ("lockdep-lite").
 *
 * There is no lock-order validator. What is checked, always (not only in
 * DEBUG builds), using the owner fields of the KPI locks:
 *   lockdep_assert_held(l)       l is held by the calling thread (for
 *                                rw_semaphore/rwlock_t: write-held by the
 *                                caller, or read-held by anyone, since
 *                                readers are not tracked per thread)
 *   lockdep_assert_held_write(l) l is write-held (or held) by the caller
 *   lockdep_assert_held_read(l)  l has at least one reader
 *   lockdep_assert_not_held(l)   l is not held (write-held) by the caller
 * A failed assertion is a WARN_ON_ONCE() for that call site. Lock types
 * this header does not know (e.g. a custom lock) are accepted silently.
 * Class keys, subclasses, maps and acquire/release annotations are no-ops.
 */
#ifndef _LINUXKPI_LINUX_LOCKDEP_H_
#define	_LINUXKPI_LINUX_LOCKDEP_H_

#include <linuxkpi/platform.h>
#include <linux/compiler.h>
#include <linux/bug.h>

struct lock_class_key {
	char	lkpi_dummy;
};

struct lockdep_map {
	char	lkpi_dummy;
};

struct pin_cookie {
	int	lkpi_dummy;
};

#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/rwsem.h>

static inline bool
lkpi_lock_held_unknown(const void *l)
{
	(void) l;
	return (true);
}

static inline bool
lkpi_lock_not_held_unknown(const void *l)
{
	(void) l;
	return (false);
}

/* Held by the caller (or, for reader/writer locks, read-held by anyone). */
#define	lockdep_is_held(l) _Generic((l),				\
	struct mutex *: lkpi_mutex_held,				\
	const struct mutex *: lkpi_mutex_held,				\
	spinlock_t *: lkpi_spin_held,					\
	const spinlock_t *: lkpi_spin_held,				\
	struct rw_semaphore *: lkpi_rwsem_held,				\
	const struct rw_semaphore *: lkpi_rwsem_held,			\
	rwlock_t *: lkpi_rwlock_held,					\
	const rwlock_t *: lkpi_rwlock_held,				\
	default: lkpi_lock_held_unknown)(l)

/* Exclusively held by the caller. */
#define	lkpi_lockdep_is_held_excl(l, unknown) _Generic((l),		\
	struct mutex *: lkpi_mutex_held,				\
	const struct mutex *: lkpi_mutex_held,				\
	spinlock_t *: lkpi_spin_held,					\
	const spinlock_t *: lkpi_spin_held,				\
	struct rw_semaphore *: lkpi_rwsem_write_held,			\
	const struct rw_semaphore *: lkpi_rwsem_write_held,		\
	rwlock_t *: lkpi_rwlock_write_held,				\
	const rwlock_t *: lkpi_rwlock_write_held,			\
	default: unknown)(l)

#define	lkpi_lockdep_is_read_held(l) _Generic((l),			\
	struct rw_semaphore *: lkpi_rwsem_read_held,			\
	const struct rw_semaphore *: lkpi_rwsem_read_held,		\
	default: lkpi_lock_held_unknown)(l)

#define	lockdep_assert(cond)		WARN_ON_ONCE(!(cond))
#define	lockdep_assert_once(cond)	WARN_ON_ONCE(!(cond))
#define	lockdep_assert_held(l)		WARN_ON_ONCE(!lockdep_is_held(l))
#define	lockdep_assert_held_once(l)	WARN_ON_ONCE(!lockdep_is_held(l))
#define	lockdep_assert_held_write(l)					\
	WARN_ON_ONCE(!lkpi_lockdep_is_held_excl(l, lkpi_lock_held_unknown))
#define	lockdep_assert_held_read(l)					\
	WARN_ON_ONCE(!lkpi_lockdep_is_read_held(l))
#define	lockdep_assert_not_held(l)					\
	WARN_ON_ONCE(lkpi_lockdep_is_held_excl(l, lkpi_lock_not_held_unknown))
#define	lockdep_assert_none_held_once()	do { } while (0)

#define	lockdep_assert_irqs_enabled()		do { } while (0)
#define	lockdep_assert_irqs_disabled()		do { } while (0)
#define	lockdep_assert_in_irq()			do { } while (0)
#define	lockdep_assert_preemption_enabled()	do { } while (0)
#define	lockdep_assert_preemption_disabled()	do { } while (0)
#define	lockdep_assert_in_softirq()		do { } while (0)

#define	lockdep_set_class(l, key)		do { (void)(key); } while (0)
#define	lockdep_set_class_and_name(l, key, n)	do { (void)(key); } while (0)
#define	lockdep_set_class_and_subclass(l, k, s)	do { (void)(k); } while (0)
#define	lockdep_set_subclass(l, sub)		do { } while (0)
#define	lockdep_set_novalidate_class(l)		do { } while (0)
#define	lockdep_init_map(map, name, key, sub)	do { } while (0)
#define	lockdep_register_key(key)		do { } while (0)
#define	lockdep_unregister_key(key)		do { } while (0)
#define	lockdep_off()				do { } while (0)
#define	lockdep_on()				do { } while (0)
#define	SINGLE_DEPTH_NESTING			1
#define	lock_acquire(...)			do { } while (0)
#define	lock_release(...)			do { } while (0)
#define	lock_acquire_shared(...)		do { } while (0)
#define	lock_acquire_exclusive(...)		do { } while (0)
/* No lockdep: the arguments (often &lock.dep_map) are never expanded. */
#define	mutex_acquire(...)			do { } while (0)
#define	mutex_acquire_nest(...)			do { } while (0)
#define	mutex_release(...)			do { } while (0)
#define	lock_acquire_shared_recursive(...)	do { } while (0)
#define	lock_map_acquire(map)			do { } while (0)
#define	lock_map_acquire_read(map)		do { } while (0)
#define	lock_map_release(map)			do { } while (0)
#define	lockdep_pin_lock(l)			((struct pin_cookie){ 0 })
#define	lockdep_unpin_lock(l, c)		do { (void)(c); } while (0)
#define	lockdep_repin_lock(l, c)		do { (void)(c); } while (0)
#define	might_lock(l)				do { (void)(l); } while (0)
#define	might_lock_read(l)			do { (void)(l); } while (0)
#define	might_lock_nested(l, s)			do { (void)(l); } while (0)
#define	STATIC_LOCKDEP_MAP_INIT(name, key)	{ 0 }

#endif /* _LINUXKPI_LINUX_LOCKDEP_H_ */
