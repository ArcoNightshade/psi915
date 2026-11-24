/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation from the documented behaviour of the Linux
 * atomic_t API (Documentation/atomic_t.txt semantics); operation set
 * cross-checked against FreeBSD's LinuxKPI asm/atomic.h and
 * asm/atomic64.h (BSD-2-Clause).
 *
 * All operations are built on the GCC/Clang __atomic builtins, which on
 * amd64 compile to inline lock-prefixed instructions in both the illumos
 * kernel (gcc -mcmodel=kernel -mno-sse) and userland; they are equivalent
 * to illumos atomic_ops(9F) (atomic_add_32_nv etc.) without the call.
 *
 * Ordering follows Linux:
 *   - atomic_read/atomic_set and void RMW ops (atomic_inc, atomic_or, ...)
 *     are relaxed (unordered);
 *   - value-returning RMW ops (atomic_inc_return, atomic_cmpxchg,
 *     atomic_fetch_or, ...) are fully ordered;
 *   - _relaxed/_acquire/_release suffixed variants have that ordering;
 *   - a failed cmpxchg gives no ordering guarantee.
 */
#ifndef _LINUXKPI_LINUX_ATOMIC_H_
#define	_LINUXKPI_LINUX_ATOMIC_H_

#include <linuxkpi/platform.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <asm/barrier.h>

typedef struct {
	int counter;
} atomic_t;

typedef struct {
	int64_t counter;
} atomic64_t;

typedef struct {
	long counter;
} atomic_long_t;

#define	ATOMIC_INIT(i)		{ (i) }
#define	ATOMIC64_INIT(i)	{ (i) }
#define	ATOMIC_LONG_INIT(i)	{ (i) }

/*
 * Value-returning operations, generated once per memory order:
 * sfx is "", _relaxed, _acquire or _release; mo/mof the success/failure
 * orders for compare-exchange.
 */
#define	__LKPI_ATOMIC_ORDERED_OPS(pfx, atype, ctype, sfx, mo, mof)	\
static inline ctype							\
pfx##_add_return##sfx(ctype i, atype *v)				\
{									\
	return (__atomic_add_fetch(&v->counter, i, mo));		\
}									\
static inline ctype							\
pfx##_sub_return##sfx(ctype i, atype *v)				\
{									\
	return (__atomic_sub_fetch(&v->counter, i, mo));		\
}									\
static inline ctype							\
pfx##_inc_return##sfx(atype *v)						\
{									\
	return (__atomic_add_fetch(&v->counter, 1, mo));		\
}									\
static inline ctype							\
pfx##_dec_return##sfx(atype *v)						\
{									\
	return (__atomic_sub_fetch(&v->counter, 1, mo));		\
}									\
static inline ctype							\
pfx##_fetch_add##sfx(ctype i, atype *v)					\
{									\
	return (__atomic_fetch_add(&v->counter, i, mo));		\
}									\
static inline ctype							\
pfx##_fetch_sub##sfx(ctype i, atype *v)					\
{									\
	return (__atomic_fetch_sub(&v->counter, i, mo));		\
}									\
static inline ctype							\
pfx##_fetch_inc##sfx(atype *v)						\
{									\
	return (__atomic_fetch_add(&v->counter, 1, mo));		\
}									\
static inline ctype							\
pfx##_fetch_dec##sfx(atype *v)						\
{									\
	return (__atomic_fetch_sub(&v->counter, 1, mo));		\
}									\
static inline ctype							\
pfx##_fetch_and##sfx(ctype i, atype *v)					\
{									\
	return (__atomic_fetch_and(&v->counter, i, mo));		\
}									\
static inline ctype							\
pfx##_fetch_andnot##sfx(ctype i, atype *v)				\
{									\
	return (__atomic_fetch_and(&v->counter, ~i, mo));		\
}									\
static inline ctype							\
pfx##_fetch_or##sfx(ctype i, atype *v)					\
{									\
	return (__atomic_fetch_or(&v->counter, i, mo));			\
}									\
static inline ctype							\
pfx##_fetch_xor##sfx(ctype i, atype *v)					\
{									\
	return (__atomic_fetch_xor(&v->counter, i, mo));		\
}									\
static inline ctype							\
pfx##_xchg##sfx(atype *v, ctype n)					\
{									\
	return (__atomic_exchange_n(&v->counter, n, mo));		\
}									\
/* Returns the value found; the store happened iff that equals o. */	\
static inline ctype							\
pfx##_cmpxchg##sfx(atype *v, ctype o, ctype n)				\
{									\
	(void) __atomic_compare_exchange_n(&v->counter, &o, n, false,	\
	    mo, mof);							\
	return (o);							\
}									\
/* On failure *o is updated to the value found. */			\
static inline bool							\
pfx##_try_cmpxchg##sfx(atype *v, ctype *o, ctype n)			\
{									\
	return (__atomic_compare_exchange_n(&v->counter, o, n, false,	\
	    mo, mof));							\
}

#define	__LKPI_ATOMIC_OPS(pfx, atype, ctype)				\
__LKPI_ATOMIC_ORDERED_OPS(pfx, atype, ctype, ,				\
    __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)					\
__LKPI_ATOMIC_ORDERED_OPS(pfx, atype, ctype, _relaxed,			\
    __ATOMIC_RELAXED, __ATOMIC_RELAXED)					\
__LKPI_ATOMIC_ORDERED_OPS(pfx, atype, ctype, _acquire,			\
    __ATOMIC_ACQUIRE, __ATOMIC_ACQUIRE)					\
__LKPI_ATOMIC_ORDERED_OPS(pfx, atype, ctype, _release,			\
    __ATOMIC_RELEASE, __ATOMIC_RELAXED)					\
static inline ctype							\
pfx##_read(const atype *v)						\
{									\
	return (__atomic_load_n(&v->counter, __ATOMIC_RELAXED));	\
}									\
static inline ctype							\
pfx##_read_acquire(const atype *v)					\
{									\
	return (__atomic_load_n(&v->counter, __ATOMIC_ACQUIRE));	\
}									\
static inline void							\
pfx##_set(atype *v, ctype i)						\
{									\
	__atomic_store_n(&v->counter, i, __ATOMIC_RELAXED);		\
}									\
static inline void							\
pfx##_set_release(atype *v, ctype i)					\
{									\
	__atomic_store_n(&v->counter, i, __ATOMIC_RELEASE);		\
}									\
static inline void							\
pfx##_add(ctype i, atype *v)						\
{									\
	(void) __atomic_fetch_add(&v->counter, i, __ATOMIC_RELAXED);	\
}									\
static inline void							\
pfx##_sub(ctype i, atype *v)						\
{									\
	(void) __atomic_fetch_sub(&v->counter, i, __ATOMIC_RELAXED);	\
}									\
static inline void							\
pfx##_inc(atype *v)							\
{									\
	(void) __atomic_fetch_add(&v->counter, 1, __ATOMIC_RELAXED);	\
}									\
static inline void							\
pfx##_dec(atype *v)							\
{									\
	(void) __atomic_fetch_sub(&v->counter, 1, __ATOMIC_RELAXED);	\
}									\
static inline void							\
pfx##_and(ctype i, atype *v)						\
{									\
	(void) __atomic_fetch_and(&v->counter, i, __ATOMIC_RELAXED);	\
}									\
static inline void							\
pfx##_andnot(ctype i, atype *v)						\
{									\
	(void) __atomic_fetch_and(&v->counter, ~i, __ATOMIC_RELAXED);	\
}									\
static inline void							\
pfx##_or(ctype i, atype *v)						\
{									\
	(void) __atomic_fetch_or(&v->counter, i, __ATOMIC_RELAXED);	\
}									\
static inline void							\
pfx##_xor(ctype i, atype *v)						\
{									\
	(void) __atomic_fetch_xor(&v->counter, i, __ATOMIC_RELAXED);	\
}									\
static inline bool							\
pfx##_sub_and_test(ctype i, atype *v)					\
{									\
	return (pfx##_sub_return(i, v) == 0);				\
}									\
static inline bool							\
pfx##_dec_and_test(atype *v)						\
{									\
	return (pfx##_dec_return(v) == 0);				\
}									\
static inline bool							\
pfx##_inc_and_test(atype *v)						\
{									\
	return (pfx##_inc_return(v) == 0);				\
}									\
static inline bool							\
pfx##_add_negative(ctype i, atype *v)					\
{									\
	return (pfx##_add_return(i, v) < 0);				\
}									\
/* Add a unless the value is u; return the old value. */		\
static inline ctype							\
pfx##_fetch_add_unless(atype *v, ctype a, ctype u)			\
{									\
	ctype c = pfx##_read(v);					\
									\
	do {								\
		if (unlikely(c == u))					\
			break;						\
	} while (!pfx##_try_cmpxchg(v, &c, c + a));			\
	return (c);							\
}									\
static inline bool							\
pfx##_add_unless(atype *v, ctype a, ctype u)				\
{									\
	return (pfx##_fetch_add_unless(v, a, u) != u);			\
}									\
static inline bool							\
pfx##_inc_not_zero(atype *v)						\
{									\
	return (pfx##_add_unless(v, 1, 0));				\
}									\
static inline bool							\
pfx##_inc_unless_negative(atype *v)					\
{									\
	ctype c = pfx##_read(v);					\
									\
	do {								\
		if (unlikely(c < 0))					\
			return (false);					\
	} while (!pfx##_try_cmpxchg(v, &c, c + 1));			\
	return (true);							\
}									\
static inline bool							\
pfx##_dec_unless_positive(atype *v)					\
{									\
	ctype c = pfx##_read(v);					\
									\
	do {								\
		if (unlikely(c > 0))					\
			return (false);					\
	} while (!pfx##_try_cmpxchg(v, &c, c - 1));			\
	return (true);							\
}									\
/* Decrement if the result stays >= 0; returns old value - 1 anyway. */	\
static inline ctype							\
pfx##_dec_if_positive(atype *v)						\
{									\
	ctype c = pfx##_read(v);					\
									\
	do {								\
		if (unlikely(c - 1 < 0))				\
			break;						\
	} while (!pfx##_try_cmpxchg(v, &c, c - 1));			\
	return (c - 1);							\
}

__LKPI_ATOMIC_OPS(atomic, atomic_t, int)
__LKPI_ATOMIC_OPS(atomic64, atomic64_t, int64_t)
__LKPI_ATOMIC_OPS(atomic_long, atomic_long_t, long)

/*
 * Generic operations on any naturally aligned 1, 2, 4 or 8 byte scalar or
 * pointer lvalue. cmpxchg returns the value found (the swap happened iff it
 * equals o); try_cmpxchg returns a bool and updates *po on failure.
 */
#define	__lkpi_xchg(p, n, mo)						\
	__atomic_exchange_n((p), (typeof(*(p)))(n), mo)

#define	__lkpi_cmpxchg(p, o, n, mo, mof) ({				\
	typeof(*(p)) __lkpi_cx_old = (o);				\
	(void) __atomic_compare_exchange_n((p), &__lkpi_cx_old,		\
	    (typeof(*(p)))(n), false, mo, mof);				\
	__lkpi_cx_old;							\
})

#define	__lkpi_try_cmpxchg(p, po, n, mo, mof)				\
	__atomic_compare_exchange_n((p), (po), (typeof(*(p)))(n), false, mo, mof)

#define	xchg(p, n)		__lkpi_xchg(p, n, __ATOMIC_SEQ_CST)
#define	xchg_relaxed(p, n)	__lkpi_xchg(p, n, __ATOMIC_RELAXED)
#define	xchg_acquire(p, n)	__lkpi_xchg(p, n, __ATOMIC_ACQUIRE)
#define	xchg_release(p, n)	__lkpi_xchg(p, n, __ATOMIC_RELEASE)

#define	cmpxchg(p, o, n)						\
	__lkpi_cmpxchg(p, o, n, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)
#define	cmpxchg_relaxed(p, o, n)					\
	__lkpi_cmpxchg(p, o, n, __ATOMIC_RELAXED, __ATOMIC_RELAXED)
#define	cmpxchg_acquire(p, o, n)					\
	__lkpi_cmpxchg(p, o, n, __ATOMIC_ACQUIRE, __ATOMIC_ACQUIRE)
#define	cmpxchg_release(p, o, n)					\
	__lkpi_cmpxchg(p, o, n, __ATOMIC_RELEASE, __ATOMIC_RELAXED)
#define	cmpxchg64(p, o, n)		cmpxchg(p, o, n)
#define	cmpxchg64_relaxed(p, o, n)	cmpxchg_relaxed(p, o, n)
#define	cmpxchg_local(p, o, n)		cmpxchg(p, o, n)
#define	cmpxchg64_local(p, o, n)	cmpxchg(p, o, n)

#define	try_cmpxchg(p, po, n)						\
	__lkpi_try_cmpxchg(p, po, n, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)
#define	try_cmpxchg_relaxed(p, po, n)					\
	__lkpi_try_cmpxchg(p, po, n, __ATOMIC_RELAXED, __ATOMIC_RELAXED)
#define	try_cmpxchg_acquire(p, po, n)					\
	__lkpi_try_cmpxchg(p, po, n, __ATOMIC_ACQUIRE, __ATOMIC_ACQUIRE)
#define	try_cmpxchg_release(p, po, n)					\
	__lkpi_try_cmpxchg(p, po, n, __ATOMIC_RELEASE, __ATOMIC_RELAXED)
#define	try_cmpxchg64(p, po, n)		try_cmpxchg(p, po, n)

/*
 * Spin until cond (which may refer to VAL, the freshly loaded *p) is true;
 * evaluates to the final VAL.
 */
#define	smp_cond_load_relaxed(p, cond) ({				\
	typeof(*(p)) VAL;						\
	for (;;) {							\
		VAL = READ_ONCE(*(p));					\
		if (cond)						\
			break;						\
		lkpi_cpu_relax();					\
	}								\
	VAL;								\
})

#define	smp_cond_load_acquire(p, cond) ({				\
	typeof(*(p)) __lkpi_scl_v = smp_cond_load_relaxed(p, cond);	\
	smp_acquire__after_ctrl_dep();					\
	__lkpi_scl_v;							\
})

#define	atomic_cond_read_relaxed(v, c)	smp_cond_load_relaxed(&(v)->counter, (c))
#define	atomic_cond_read_acquire(v, c)	smp_cond_load_acquire(&(v)->counter, (c))
#define	atomic64_cond_read_relaxed(v, c) smp_cond_load_relaxed(&(v)->counter, (c))
#define	atomic64_cond_read_acquire(v, c) smp_cond_load_acquire(&(v)->counter, (c))

#endif /* _LINUXKPI_LINUX_ATOMIC_H_ */
