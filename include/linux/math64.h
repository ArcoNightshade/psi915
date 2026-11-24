/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation from the documented behaviour of the Linux
 * kernel API; function set cross-checked against FreeBSD's LinuxKPI
 * math64.h (BSD-2-Clause).
 *
 * amd64 divides 64-bit values natively. 128-bit products use unsigned
 * __int128 multiplication and shifts, which compile inline; 128-bit
 * division would call libgcc (__udivti3), absent from the illumos kernel,
 * so mul_u64_u64_div_u64 uses divq (or a shift-subtract loop elsewhere).
 */
#ifndef _LINUXKPI_LINUX_MATH64_H_
#define	_LINUXKPI_LINUX_MATH64_H_

#include <linuxkpi/platform.h>
#include <linux/types.h>
#include <linux/math.h>

static inline u64
div_u64_rem(u64 dividend, u32 divisor, u32 *remainder)
{
	*remainder = (u32)(dividend % divisor);
	return (dividend / divisor);
}

static inline s64
div_s64_rem(s64 dividend, s32 divisor, s32 *remainder)
{
	*remainder = (s32)(dividend % divisor);
	return (dividend / divisor);
}

static inline u64
div64_u64_rem(u64 dividend, u64 divisor, u64 *remainder)
{
	*remainder = dividend % divisor;
	return (dividend / divisor);
}

static inline u64
div_u64(u64 dividend, u32 divisor)
{
	return (dividend / divisor);
}

static inline s64
div_s64(s64 dividend, s32 divisor)
{
	return (dividend / divisor);
}

static inline u64
div64_u64(u64 dividend, u64 divisor)
{
	return (dividend / divisor);
}

static inline s64
div64_s64(s64 dividend, s64 divisor)
{
	return (dividend / divisor);
}

#define	div64_ul(x, y)		div64_u64((x), (y))
#define	div64_long(x, y)	div64_s64((x), (y))

/* Divide the 64-bit lvalue n by base in place; evaluates to the remainder. */
#define	do_div(n, base) ({						\
	u32 __lkpi_dd_base = (base);					\
	u32 __lkpi_dd_rem = (u32)((u64)(n) % __lkpi_dd_base);		\
	(n) = (u64)(n) / __lkpi_dd_base;				\
	__lkpi_dd_rem;							\
})

static inline u64
mul_u32_u32(u32 a, u32 b)
{
	return ((u64)a * b);
}

static inline u64
mul_u64_u32_shr(u64 a, u32 mul, unsigned int shift)
{
	return ((u64)(((unsigned __int128)a * mul) >> shift));
}

static inline u64
mul_u64_u64_shr(u64 a, u64 mul, unsigned int shift)
{
	return ((u64)(((unsigned __int128)a * mul) >> shift));
}

/* (a * b) / c with a 128-bit intermediate; the quotient must fit in u64. */
static inline u64
mul_u64_u64_div_u64(u64 a, u64 b, u64 c)
{
	unsigned __int128 p = (unsigned __int128)a * b;
	u64 hi = (u64)(p >> 64), lo = (u64)p;
#if defined(__x86_64__) || defined(__amd64)
	u64 q, r;

	if (hi < c) {
		__asm__("divq %4" : "=a"(q), "=d"(r) : "a"(lo), "d"(hi), "rm"(c));
		return (q);
	}
#endif
	{
		/* Shift-subtract long division; keeps the low 64 bits. */
		unsigned __int128 rem = 0, quo = 0;
		int i;

		for (i = 127; i >= 0; i--) {
			rem = (rem << 1) | (u64)((p >> i) & 1);
			if (rem >= c) {
				rem -= c;
				quo |= (unsigned __int128)1 << i;
			}
		}
		return ((u64)quo);
	}
}

static inline u64
mul_u64_u32_div(u64 a, u32 mul, u32 divisor)
{
	return (mul_u64_u64_div_u64(a, mul, divisor));
}

#define	DIV64_U64_ROUND_UP(ll, d)					\
	({ u64 __lkpi_d = (d); div64_u64((ll) + __lkpi_d - 1, __lkpi_d); })

#define	DIV64_U64_ROUND_CLOSEST(dividend, divisor)			\
	({ u64 __lkpi_dv = (divisor);					\
	    div64_u64((dividend) + __lkpi_dv / 2, __lkpi_dv); })

#define	DIV_U64_ROUND_UP(ll, d)						\
	({ u32 __lkpi_d32 = (d); div_u64((ll) + __lkpi_d32 - 1, __lkpi_d32); })

#define	DIV_U64_ROUND_CLOSEST(dividend, divisor)			\
	({ u32 __lkpi_dv32 = (divisor);					\
	    div_u64((u64)(dividend) + (__lkpi_dv32 / 2), __lkpi_dv32); })

#endif /* _LINUXKPI_LINUX_MATH64_H_ */
