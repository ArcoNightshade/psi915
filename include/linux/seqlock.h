/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation from the documented behaviour of the Linux
 * seqcount/seqlock API (Documentation/locking/seqlock.rst semantics);
 * cross-checked against FreeBSD's LinuxKPI seqlock.h (BSD-2-Clause).
 *
 * The seqcount_LOCKNAME_t variants (seqcount_spinlock_t, seqcount_mutex_t,
 * ...) all start with a seqcount_t, so every seqcount operation accepts a
 * pointer to any of them. Writers must be serialised by the caller (or by
 * the associated lock); the associated lock is recorded but not checked.
 */
#ifndef _LINUXKPI_LINUX_SEQLOCK_H_
#define	_LINUXKPI_LINUX_SEQLOCK_H_

#include <linuxkpi/platform.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <asm/barrier.h>

typedef struct seqcount {
	unsigned int	sequence;
} seqcount_t;

#define	SEQCNT_ZERO(name)	{ .sequence = 0 }

#define	__LKPI_SEQCOUNT_LOCKED(lname, ltype)				\
typedef struct seqcount_##lname {					\
	seqcount_t	seqcount;					\
	ltype		*lock;						\
} seqcount_##lname##_t;							\
static inline void							\
seqcount_##lname##_init(seqcount_##lname##_t *s, ltype *l)		\
{									\
	s->seqcount.sequence = 0;					\
	s->lock = l;							\
}

struct mutex;
struct ww_mutex;
__LKPI_SEQCOUNT_LOCKED(raw_spinlock, raw_spinlock_t)
__LKPI_SEQCOUNT_LOCKED(spinlock, spinlock_t)
__LKPI_SEQCOUNT_LOCKED(rwlock, rwlock_t)
__LKPI_SEQCOUNT_LOCKED(mutex, struct mutex)
__LKPI_SEQCOUNT_LOCKED(ww_mutex, struct ww_mutex)

#define	SEQCNT_SPINLOCK_ZERO(name, l)	{ .seqcount = SEQCNT_ZERO(name), .lock = (l) }
#define	SEQCNT_RAW_SPINLOCK_ZERO(name, l) SEQCNT_SPINLOCK_ZERO(name, l)
#define	SEQCNT_RWLOCK_ZERO(name, l)	SEQCNT_SPINLOCK_ZERO(name, l)
#define	SEQCNT_MUTEX_ZERO(name, l)	SEQCNT_SPINLOCK_ZERO(name, l)
#define	SEQCNT_WW_MUTEX_ZERO(name, l)	SEQCNT_SPINLOCK_ZERO(name, l)

/* Any seqcount variant -> its seqcount_t (always the first member). */
#define	__lkpi_seqc(s)	((seqcount_t *)(void *)(s))

static inline void
lkpi_seqcount_init(seqcount_t *s)
{
	s->sequence = 0;
}
#define	seqcount_init(s)	lkpi_seqcount_init(__lkpi_seqc(s))

/* Wait for an even (no writer) sequence; no barrier. */
static inline unsigned int
__lkpi_read_seqcount_begin(const seqcount_t *s)
{
	unsigned int seq;

	while ((seq = READ_ONCE(s->sequence)) & 1)
		lkpi_cpu_relax();
	return (seq);
}

static inline unsigned int
__lkpi_raw_read_seqcount_begin(const seqcount_t *s)
{
	unsigned int seq = __lkpi_read_seqcount_begin(s);

	smp_rmb();
	return (seq);
}

/* Sequence without waiting for writers; may be odd. */
static inline unsigned int
__lkpi_raw_read_seqcount(const seqcount_t *s)
{
	unsigned int seq = READ_ONCE(s->sequence);

	smp_rmb();
	return (seq);
}

static inline int
__lkpi_read_seqcount_retry(const seqcount_t *s, unsigned int start)
{
	smp_rmb();
	return (unlikely(READ_ONCE(s->sequence) != start));
}

static inline void
__lkpi_raw_write_seqcount_begin(seqcount_t *s)
{
	WRITE_ONCE(s->sequence, s->sequence + 1);
	smp_wmb();
}

static inline void
__lkpi_raw_write_seqcount_end(seqcount_t *s)
{
	smp_wmb();
	WRITE_ONCE(s->sequence, s->sequence + 1);
}

static inline void
__lkpi_write_seqcount_invalidate(seqcount_t *s)
{
	smp_wmb();
	WRITE_ONCE(s->sequence, s->sequence + 2);
}

/* Current sequence (Linux internal accessor, used by i915). */
#define	seqprop_sequence(s)	READ_ONCE(__lkpi_seqc(s)->sequence)
#define	__read_seqcount_begin(s)	__lkpi_read_seqcount_begin(__lkpi_seqc(s))
#define	raw_read_seqcount_begin(s)	__lkpi_raw_read_seqcount_begin(__lkpi_seqc(s))
#define	read_seqcount_begin(s)		__lkpi_raw_read_seqcount_begin(__lkpi_seqc(s))
#define	raw_read_seqcount(s)		__lkpi_raw_read_seqcount(__lkpi_seqc(s))
#define	raw_seqcount_begin(s)		(raw_read_seqcount(s) & ~1U)
#define	__read_seqcount_retry(s, start)					\
	(READ_ONCE(__lkpi_seqc(s)->sequence) != (start))
#define	read_seqcount_retry(s, start)					\
	__lkpi_read_seqcount_retry(__lkpi_seqc(s), start)
#define	raw_write_seqcount_begin(s)	__lkpi_raw_write_seqcount_begin(__lkpi_seqc(s))
#define	raw_write_seqcount_end(s)	__lkpi_raw_write_seqcount_end(__lkpi_seqc(s))
#define	write_seqcount_begin(s)		__lkpi_raw_write_seqcount_begin(__lkpi_seqc(s))
#define	write_seqcount_begin_nested(s, sub) write_seqcount_begin(s)
#define	write_seqcount_end(s)		__lkpi_raw_write_seqcount_end(__lkpi_seqc(s))
#define	write_seqcount_invalidate(s)	__lkpi_write_seqcount_invalidate(__lkpi_seqc(s))
#define	raw_write_seqcount_barrier(s) do {				\
	raw_write_seqcount_begin(s);					\
	raw_write_seqcount_end(s);					\
} while (0)

/* seqlock_t: a seqcount with its own spinlock serialising writers. */
typedef struct {
	seqcount_spinlock_t	seqcount;
	spinlock_t		lock;
} seqlock_t;

#define	__SEQLOCK_UNLOCKED(name)					\
	{ .seqcount = { .seqcount = SEQCNT_ZERO(name), .lock = &(name).lock }, \
	    .lock = __SPIN_LOCK_UNLOCKED(name) }
#define	DEFINE_SEQLOCK(name)	seqlock_t name = __SEQLOCK_UNLOCKED(name)

static inline void
seqlock_init(seqlock_t *sl)
{
	spin_lock_init(&sl->lock);
	seqcount_spinlock_init(&sl->seqcount, &sl->lock);
}

static inline unsigned int
read_seqbegin(const seqlock_t *sl)
{
	return (read_seqcount_begin(&sl->seqcount));
}

static inline int
read_seqretry(const seqlock_t *sl, unsigned int start)
{
	return (read_seqcount_retry(&sl->seqcount, start));
}

static inline void
write_seqlock(seqlock_t *sl)
{
	spin_lock(&sl->lock);
	write_seqcount_begin(&sl->seqcount);
}

static inline void
write_sequnlock(seqlock_t *sl)
{
	write_seqcount_end(&sl->seqcount);
	spin_unlock(&sl->lock);
}

#define	write_seqlock_irq(sl)		write_seqlock(sl)
#define	write_sequnlock_irq(sl)		write_sequnlock(sl)
#define	write_seqlock_bh(sl)		write_seqlock(sl)
#define	write_sequnlock_bh(sl)		write_sequnlock(sl)
#define	write_seqlock_irqsave(sl, flags)				\
	do { (flags) = 0; write_seqlock(sl); } while (0)
#define	write_sequnlock_irqrestore(sl, flags)				\
	do { (void)(flags); write_sequnlock(sl); } while (0)

/* Exclusive reader: blocks writers and other exclusive readers. */
static inline void
read_seqlock_excl(seqlock_t *sl)
{
	spin_lock(&sl->lock);
}

static inline void
read_sequnlock_excl(seqlock_t *sl)
{
	spin_unlock(&sl->lock);
}

#define	read_seqlock_excl_irqsave(sl, flags)				\
	do { (flags) = 0; read_seqlock_excl(sl); } while (0)
#define	read_sequnlock_excl_irqrestore(sl, flags)			\
	do { (void)(flags); read_sequnlock_excl(sl); } while (0)

/*
 * Lockless first pass (*seq even), locked second pass (*seq odd):
 *   int seq = 0;
 *   again: read_seqbegin_or_lock(sl, &seq); ...
 *   if (need_seqretry(sl, seq)) { seq = 1; goto again; }
 *   done_seqretry(sl, seq);
 */
static inline void
read_seqbegin_or_lock(seqlock_t *sl, int *seq)
{
	if (!(*seq & 1))
		*seq = (int)read_seqbegin(sl);
	else
		read_seqlock_excl(sl);
}

static inline int
need_seqretry(seqlock_t *sl, int seq)
{
	return (!(seq & 1) && read_seqretry(sl, (unsigned int)seq));
}

static inline void
done_seqretry(seqlock_t *sl, int seq)
{
	if (seq & 1)
		read_sequnlock_excl(sl);
}

#endif /* _LINUXKPI_LINUX_SEQLOCK_H_ */
