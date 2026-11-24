/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation from the documented behaviour of the Linux
 * spinlock_t and rwlock_t APIs; function set cross-checked against
 * FreeBSD's LinuxKPI spinlock.h and rwlock.h (BSD-2-Clause).
 *
 * spinlock_t is an illumos kmutex_t plus an owner field for lockdep-lite
 * assertions (<linux/lockdep.h>). Holding one never disables interrupts or
 * preemption, and the holder may block on it (adaptive mutex); code that
 * relies on "holding a spinlock means nobody else runs on this CPU" is
 * not supported.
 *
 * Interrupts. spin_lock_irqsave()/spin_lock_irq()/spin_lock_bh() just take
 * the mutex (flags is set to 0). That is correct on illumos because
 * interrupt handlers below LOCK_LEVEL run as threads and may block on an
 * adaptive mutex. A lock that a handler ABOVE LOCK_LEVEL takes must be a
 * spin mutex at that handler's priority, which Linux code cannot express;
 * the port must initialise such locks with
 *
 *   spin_lock_init_irqsafe(&lock, DDI_INTR_PRI(pri))
 *
 * where pri comes from ddi_intr_get_pri(9F) for the interrupt (the
 * iblock cookie of mutex_init(9F); the lock becomes MUTEX_DRIVER). The same
 * applies to rwlock_init_irqsafe() for completeness (krwlock_t cannot be
 * used above LOCK_LEVEL at all; such a rwlock_t must be converted to a
 * spinlock_t by the port). i915 uses MSI, which is below LOCK_LEVEL.
 *
 * rwlock_t is a krwlock_t. Unlike Linux, readers are not recursive while
 * a writer waits (krwlock prefers writers): a reader that re-takes the
 * lock behind a waiting writer deadlocks.
 */
#ifndef _LINUXKPI_LINUX_SPINLOCK_H_
#define	_LINUXKPI_LINUX_SPINLOCK_H_

#include <linuxkpi/platform.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/atomic.h>
#include <linux/refcount.h>
#include <linux/bug.h>

typedef struct spinlock {
	lkpi_plat_mutex_t	m;
	void			*owner;
} spinlock_t;

typedef spinlock_t raw_spinlock_t;

#define	__SPIN_LOCK_UNLOCKED(name)					\
	{ .m = LKPI_PLAT_MUTEX_INITIALIZER, .owner = NULL }
#define	__RAW_SPIN_LOCK_UNLOCKED(name)	__SPIN_LOCK_UNLOCKED(name)
#define	DEFINE_SPINLOCK(name)	spinlock_t name = __SPIN_LOCK_UNLOCKED(name)
#define	DEFINE_RAW_SPINLOCK(name)					\
	raw_spinlock_t name = __SPIN_LOCK_UNLOCKED(name)

void lkpi_lock_recursion(const void *lock, const char *what);

static inline void
spin_lock_init_irqsafe(spinlock_t *l, void *iblock_cookie)
{
	lkpi_plat_mutex_init(&l->m, iblock_cookie);
	l->owner = NULL;
}

static inline void
lkpi_spin_lock_init(spinlock_t *l)
{
	spin_lock_init_irqsafe(l, NULL);
}

#define	spin_lock_init(l)	lkpi_spin_lock_init(l)
#define	raw_spin_lock_init(l)	lkpi_spin_lock_init(l)

/* Not part of the Linux API (spinlocks are never destroyed there). */
static inline void
lkpi_spin_lock_destroy(spinlock_t *l)
{
	lkpi_plat_mutex_destroy(&l->m);
}

static inline void
spin_lock(spinlock_t *l)
{
	void *self = lkpi_plat_curthread();

	if (unlikely(READ_ONCE(l->owner) == self))
		lkpi_lock_recursion(l, "spinlock");
	lkpi_plat_mutex_enter(&l->m);
	WRITE_ONCE(l->owner, self);
}

static inline int
spin_trylock(spinlock_t *l)
{
	if (!lkpi_plat_mutex_tryenter(&l->m))
		return (0);
	WRITE_ONCE(l->owner, lkpi_plat_curthread());
	return (1);
}

static inline void
spin_unlock(spinlock_t *l)
{
	WRITE_ONCE(l->owner, NULL);
	lkpi_plat_mutex_exit(&l->m);
}

static inline int
spin_is_locked(spinlock_t *l)
{
	return (READ_ONCE(l->owner) != NULL);
}

static inline bool
lkpi_spin_held(const spinlock_t *l)
{
	return (READ_ONCE(l->owner) == lkpi_plat_curthread());
}

#define	spin_lock_nested(l, subclass)	spin_lock(l)
#define	spin_lock_bh(l)			spin_lock(l)
#define	spin_unlock_bh(l)		spin_unlock(l)
#define	spin_lock_irq(l)		spin_lock(l)
#define	spin_unlock_irq(l)		spin_unlock(l)
#define	spin_trylock_bh(l)		spin_trylock(l)
#define	spin_trylock_irq(l)		spin_trylock(l)

#define	spin_lock_irqsave(l, flags) do {				\
	(flags) = 0;							\
	spin_lock(l);							\
} while (0)
#define	spin_lock_irqsave_nested(l, flags, subclass)			\
	spin_lock_irqsave(l, flags)
#define	spin_unlock_irqrestore(l, flags) do {				\
	(void)(flags);							\
	spin_unlock(l);							\
} while (0)
#define	spin_trylock_irqsave(l, flags)					\
	({ (flags) = 0; spin_trylock(l); })

#define	spin_is_contended(l)		(0)
#define	spin_needbreak(l)		(0)
#define	assert_spin_locked(l)		WARN_ON_ONCE(!lkpi_spin_held(l))
#define	spin_unlock_wait(l)		do { spin_lock(l); spin_unlock(l); } while (0)

#define	raw_spin_lock(l)		spin_lock(l)
#define	raw_spin_unlock(l)		spin_unlock(l)
#define	raw_spin_trylock(l)		spin_trylock(l)
#define	raw_spin_lock_irq(l)		spin_lock(l)
#define	raw_spin_unlock_irq(l)		spin_unlock(l)
#define	raw_spin_lock_irqsave(l, f)	spin_lock_irqsave(l, f)
#define	raw_spin_unlock_irqrestore(l, f) spin_unlock_irqrestore(l, f)
#define	raw_spin_is_locked(l)		spin_is_locked(l)

/* Interrupt state: illumos code here never masks interrupts. */
#define	local_irq_save(flags)		do { (flags) = 0; } while (0)
#define	local_irq_restore(flags)	do { (void)(flags); } while (0)
#define	local_irq_disable()		do { } while (0)
#define	local_irq_enable()		do { } while (0)
#define	local_bh_disable()		do { } while (0)
#define	local_bh_enable()		do { } while (0)
#define	irqs_disabled()			(0)

/*
 * Reader/writer spinlock over krwlock_t. The writer is tracked for
 * lockdep-lite; readers only as a count.
 */
typedef struct {
	lkpi_plat_rwlock_t	rw;
	void			*owner;
	atomic_t		readers;
} rwlock_t;

#define	__RW_LOCK_UNLOCKED(name)					\
	{ .rw = LKPI_PLAT_RWLOCK_INITIALIZER, .owner = NULL,		\
	    .readers = ATOMIC_INIT(0) }
#define	DEFINE_RWLOCK(name)	rwlock_t name = __RW_LOCK_UNLOCKED(name)

static inline void
rwlock_init_irqsafe(rwlock_t *l, void *iblock_cookie)
{
	(void) iblock_cookie;	/* krwlock has no interrupt priority */
	lkpi_plat_rwlock_init(&l->rw);
	l->owner = NULL;
	atomic_set(&l->readers, 0);
}

#define	rwlock_init(l)		rwlock_init_irqsafe(l, NULL)

static inline void
read_lock(rwlock_t *l)
{
	lkpi_plat_rwlock_enter(&l->rw, false);
	atomic_inc(&l->readers);
}

static inline int
read_trylock(rwlock_t *l)
{
	if (!lkpi_plat_rwlock_tryenter(&l->rw, false))
		return (0);
	atomic_inc(&l->readers);
	return (1);
}

static inline void
read_unlock(rwlock_t *l)
{
	atomic_dec(&l->readers);
	lkpi_plat_rwlock_exit(&l->rw);
}

static inline void
write_lock(rwlock_t *l)
{
	void *self = lkpi_plat_curthread();

	if (unlikely(READ_ONCE(l->owner) == self))
		lkpi_lock_recursion(l, "rwlock");
	lkpi_plat_rwlock_enter(&l->rw, true);
	WRITE_ONCE(l->owner, self);
}

static inline int
write_trylock(rwlock_t *l)
{
	if (!lkpi_plat_rwlock_tryenter(&l->rw, true))
		return (0);
	WRITE_ONCE(l->owner, lkpi_plat_curthread());
	return (1);
}

static inline void
write_unlock(rwlock_t *l)
{
	WRITE_ONCE(l->owner, NULL);
	lkpi_plat_rwlock_exit(&l->rw);
}

static inline bool
lkpi_rwlock_held(const rwlock_t *l)
{
	return (READ_ONCE(l->owner) == lkpi_plat_curthread() ||
	    atomic_read(&l->readers) > 0);
}

static inline bool
lkpi_rwlock_write_held(const rwlock_t *l)
{
	return (READ_ONCE(l->owner) == lkpi_plat_curthread());
}

#define	read_lock_irq(l)		read_lock(l)
#define	read_unlock_irq(l)		read_unlock(l)
#define	read_lock_bh(l)			read_lock(l)
#define	read_unlock_bh(l)		read_unlock(l)
#define	write_lock_irq(l)		write_lock(l)
#define	write_unlock_irq(l)		write_unlock(l)
#define	write_lock_bh(l)		write_lock(l)
#define	write_unlock_bh(l)		write_unlock(l)
#define	read_lock_irqsave(l, f)		do { (f) = 0; read_lock(l); } while (0)
#define	read_unlock_irqrestore(l, f)	do { (void)(f); read_unlock(l); } while (0)
#define	write_lock_irqsave(l, f)	do { (f) = 0; write_lock(l); } while (0)
#define	write_unlock_irqrestore(l, f)	do { (void)(f); write_unlock(l); } while (0)

/*
 * Decrement; if the result would be zero, take the lock first and return
 * true with it held (the caller frees the object and unlocks).
 */
static inline bool
refcount_dec_and_lock(refcount_t *r, spinlock_t *l)
{
	if (refcount_dec_not_one(r))
		return (false);
	spin_lock(l);
	if (!refcount_dec_and_test(r)) {
		spin_unlock(l);
		return (false);
	}
	return (true);
}

#define	refcount_dec_and_lock_irqsave(r, l, flags)			\
	({ *(flags) = 0; refcount_dec_and_lock(r, l); })

static inline bool
atomic_dec_and_lock(atomic_t *a, spinlock_t *l)
{
	if (atomic_add_unless(a, -1, 1))
		return (false);
	spin_lock(l);
	if (atomic_dec_return(a) == 0)
		return (true);
	spin_unlock(l);
	return (false);
}

#define	atomic_dec_and_lock_irqsave(a, l, flags)			\
	({ (flags) = 0; atomic_dec_and_lock(a, l); })

#include <linux/lockdep.h>

#endif /* _LINUXKPI_LINUX_SPINLOCK_H_ */
