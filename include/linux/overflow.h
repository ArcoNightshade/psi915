/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation from the documented behaviour of the Linux
 * kernel API (linux/overflow.h), over the __builtin_*_overflow builtins.
 * The size_*() and struct_size() helpers saturate at SIZE_MAX, so an
 * overflowing size makes the allocation fail instead of coming out small.
 */
#ifndef _LINUXKPI_LINUX_OVERFLOW_H_
#define	_LINUXKPI_LINUX_OVERFLOW_H_

#include <linuxkpi/platform.h>
#include <linux/compiler.h>
#include <linux/limits.h>

#define	is_signed_type(t)	(((t)(-1)) < (t)1)
#define	__type_half_max(t)	((t)1 << (8 * sizeof(t) - 1 - is_signed_type(t)))
#define	type_max(t)		((t)((__type_half_max(t) - 1) + __type_half_max(t)))
#define	type_min(t)		((t)((t)-type_max(t) - (t)1))

/* Each stores the (wrapped) result in *d and returns true on overflow. */
#define	check_add_overflow(a, b, d)	__builtin_add_overflow(a, b, d)
#define	check_sub_overflow(a, b, d)	__builtin_sub_overflow(a, b, d)
#define	check_mul_overflow(a, b, d)	__builtin_mul_overflow(a, b, d)

/* Out of line so gcc's -Wtype-limits does not see "unsigned < 0". */
static inline bool
__lkpi_is_negative(long long v, bool is_signed)
{
	return (is_signed && v < 0);
}

/* *d = a << s; true if bits were lost, s is out of range or a < 0. */
#define	check_shl_overflow(a, s, d) ({					\
	typeof(a) __lkpi_a = (a);					\
	typeof(s) __lkpi_s = (s);					\
	typeof(d) __lkpi_d = (d);					\
	unsigned long long __lkpi_ss = (unsigned long long)__lkpi_s;	\
	bool __lkpi_ovf = __lkpi_ss >= 8 * sizeof(*__lkpi_d) ||		\
	    __lkpi_is_negative((long long)__lkpi_a,			\
	    is_signed_type(typeof(__lkpi_a)));				\
									\
	*__lkpi_d = __lkpi_ovf ? 0 :					\
	    (typeof(*__lkpi_d))((unsigned long long)__lkpi_a << __lkpi_ss); \
	__lkpi_ovf ||							\
	    ((typeof(*__lkpi_d))(*__lkpi_d >> __lkpi_ss) != __lkpi_a);	\
})

/* True if n does not fit in type t (or in the type of variable v). */
#define	overflows_type(n, t) ({						\
	typeof(t) __lkpi_ot;						\
	check_add_overflow((n), 0, &__lkpi_ot);				\
})

static inline size_t __must_check
size_mul(size_t a, size_t b)
{
	size_t r;

	return (__builtin_mul_overflow(a, b, &r) ? SIZE_MAX : r);
}

static inline size_t __must_check
size_add(size_t a, size_t b)
{
	size_t r;

	return (__builtin_add_overflow(a, b, &r) ? SIZE_MAX : r);
}

static inline size_t __must_check
size_sub(size_t a, size_t b)
{
	size_t r;

	if (a == SIZE_MAX || b == SIZE_MAX || __builtin_sub_overflow(a, b, &r))
		return (SIZE_MAX);
	return (r);
}

#define	array_size(a, b)	size_mul(a, b)
#define	array3_size(a, b, c)	size_mul(size_mul(a, b), c)

/* Size of count elements of p's flexible array member. */
#define	flex_array_size(p, member, count)				\
	size_mul(count, sizeof(*(p)->member))

/* Size of *p plus count elements of its flexible array member. */
#define	struct_size(p, member, count)					\
	size_add(sizeof(*(p)), flex_array_size(p, member, count))

#define	struct_size_t(type, member, count)				\
	struct_size((type *)NULL, member, count)

#endif /* _LINUXKPI_LINUX_OVERFLOW_H_ */
