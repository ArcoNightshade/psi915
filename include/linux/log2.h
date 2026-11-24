/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation from the documented behaviour of the Linux
 * kernel API; cross-checked against FreeBSD's LinuxKPI log2.h
 * (BSD-2-Clause).
 *
 * ilog2() of a constant argument folds to a constant (gcc and clang fold
 * __builtin_clzll of a constant), so it can size arrays and appear in
 * BUILD_BUG_ON. ilog2(0) is undefined, as in Linux.
 */
#ifndef _LINUXKPI_LINUX_LOG2_H_
#define	_LINUXKPI_LINUX_LOG2_H_

#include <linuxkpi/platform.h>
#include <linux/types.h>
#include <linux/bitops.h>

static inline int
__ilog2_u32(uint32_t n)
{
	return (lkpi_fls(n) - 1);
}

static inline int
__ilog2_u64(uint64_t n)
{
	return (fls64(n) - 1);
}

#define	const_ilog2(n)							\
	((n) < 2 ? 0 : 63 - __builtin_clzll((unsigned long long)(n)))

#define	ilog2(n)							\
	(__builtin_constant_p(n) ? const_ilog2(n) :			\
	    (sizeof(n) <= 4 ? __ilog2_u32(n) : __ilog2_u64(n)))

static inline bool
is_power_of_2(unsigned long n)
{
	return (n != 0 && (n & (n - 1)) == 0);
}

/* Smallest power of two >= n (n >= 1). */
static inline unsigned long
__roundup_pow_of_two(unsigned long n)
{
	return (n <= 1 ? 1UL : 1UL << fls_long(n - 1));
}

/* Largest power of two <= n (n >= 1). */
static inline unsigned long
__rounddown_pow_of_two(unsigned long n)
{
	return (1UL << (fls_long(n) - 1));
}

#define	roundup_pow_of_two(n)						\
	(__builtin_constant_p(n) ?					\
	    ((n) <= 1 ? 1UL : 1UL << (const_ilog2((n) - 1) + 1)) :	\
	    __roundup_pow_of_two(n))

#define	rounddown_pow_of_two(n)						\
	(__builtin_constant_p(n) ? 1UL << const_ilog2(n) :		\
	    __rounddown_pow_of_two(n))

/* ceil(log2(n)); 0 for n <= 1. */
#define	order_base_2(n)	((n) > 1 ? ilog2((n) - 1) + 1 : 0)

/* Number of bits needed to represent n (at least 1). */
#define	bits_per(n)	((n) < 2 ? 1 : ilog2(n) + 1)

#endif /* _LINUXKPI_LINUX_LOG2_H_ */
