/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation from the documented behaviour of the Linux
 * kernel refcount_t API.
 *
 * refcount_t wraps an atomic_t (<linux/atomic.h>), as in Linux.
 *
 * Semantics follow Linux: a counter that would increment from zero or
 * overflow, or decrement below zero, is pinned at REFCOUNT_SATURATED and a
 * warning is issued. A saturated object is leaked rather than freed early.
 */
#ifndef _LINUXKPI_LINUX_REFCOUNT_H_
#define	_LINUXKPI_LINUX_REFCOUNT_H_

#include <linuxkpi/platform.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/atomic.h>

typedef struct refcount_struct {
	atomic_t refs;
} refcount_t;

#define	REFCOUNT_INIT(n)	{ .refs = ATOMIC_INIT(n) }
#define	REFCOUNT_MAX		0x7fffffff
#define	REFCOUNT_SATURATED	((int)(0xc0000000U))	/* INT_MIN / 2 */

static inline void
__refcount_saturate(refcount_t *r, const char *why)
{
	atomic_set(&r->refs, REFCOUNT_SATURATED);
	lkpi_warn(why);
}

static inline void
refcount_set(refcount_t *r, int n)
{
	atomic_set(&r->refs, n);
}

static inline unsigned int
refcount_read(const refcount_t *r)
{
	return ((unsigned int)atomic_read(&r->refs));
}

static inline void
refcount_add(int i, refcount_t *r)
{
	int old = atomic_fetch_add_relaxed(i, &r->refs);

	if (unlikely(old == 0))
		__refcount_saturate(r, "refcount_t: addition on 0; use-after-free");
	else if (unlikely(old < 0 || (int)((unsigned int)old + i) < 0))
		__refcount_saturate(r, "refcount_t: saturated; leaking memory");
}

static inline void
refcount_inc(refcount_t *r)
{
	refcount_add(1, r);
}

/* Returns false (and does nothing) if the count was zero. */
static inline bool __must_check
refcount_add_not_zero(int i, refcount_t *r)
{
	int old = atomic_read(&r->refs);

	do {
		if (old == 0)
			return (false);
	} while (!atomic_try_cmpxchg_relaxed(&r->refs, &old,
	    (int)((unsigned int)old + i)));

	if (unlikely(old < 0 || (int)((unsigned int)old + i) < 0))
		__refcount_saturate(r, "refcount_t: saturated; leaking memory");
	return (true);
}

static inline bool __must_check
refcount_inc_not_zero(refcount_t *r)
{
	return (refcount_add_not_zero(1, r));
}

/*
 * Subtract i; return true if the count reached zero. Release ordering on
 * the decrement, acquire ordering when it hits zero, so the caller's frees
 * happen after all other holders' accesses.
 */
static inline bool __must_check
refcount_sub_and_test(int i, refcount_t *r)
{
	int old = atomic_fetch_sub_release(i, &r->refs);

	if (old == i) {
		smp_acquire__after_ctrl_dep();
		return (true);
	}
	if (unlikely(old < 0 || (int)((unsigned int)old - i) < 0))
		__refcount_saturate(r, "refcount_t: underflow; use-after-free");
	return (false);
}

static inline bool __must_check
refcount_dec_and_test(refcount_t *r)
{
	return (refcount_sub_and_test(1, r));
}

static inline void
refcount_dec(refcount_t *r)
{
	if (unlikely(refcount_dec_and_test(r)))
		__refcount_saturate(r, "refcount_t: decrement hit 0; leaking memory");
}

/* Decrement unless the count is 1; returns true if it decremented. */
static inline bool __must_check
refcount_dec_not_one(refcount_t *r)
{
	int old = atomic_read(&r->refs);

	do {
		if (unlikely(old == REFCOUNT_SATURATED))
			return (true);
		if (old == 1)
			return (false);
		if (unlikely(old == 0)) {
			lkpi_warn("refcount_t: underflow; use-after-free");
			return (false);
		}
	} while (!atomic_try_cmpxchg_release(&r->refs, &old, old - 1));
	return (true);
}

#endif /* _LINUXKPI_LINUX_REFCOUNT_H_ */
