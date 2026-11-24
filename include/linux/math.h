/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation from the documented behaviour of the Linux
 * kernel API (linux/math.h); cross-checked against FreeBSD's LinuxKPI
 * math.h (BSD-2-Clause).
 */
#ifndef _LINUXKPI_LINUX_MATH_H_
#define	_LINUXKPI_LINUX_MATH_H_

#include <linuxkpi/platform.h>
#include <linux/compiler.h>
#include <linux/types.h>

#ifndef DIV_ROUND_UP
#define	DIV_ROUND_UP(n, d)	(((n) + (d) - 1) / (d))
#endif
#define	DIV_ROUND_DOWN_ULL(ll, d)					\
	({ unsigned long long __lkpi_ll = (ll); __lkpi_ll / (d); })
#define	DIV_ROUND_UP_ULL(ll, d)						\
	DIV_ROUND_DOWN_ULL((unsigned long long)(ll) + (d) - 1, (d))

/*
 * Round to nearest; halves round away from zero. Works for signed
 * dividends with a positive divisor.
 */
#define	DIV_ROUND_CLOSEST(x, divisor) ({				\
	typeof(x) __lkpi_x = (x);					\
	typeof(divisor) __lkpi_d = (divisor);				\
	(((typeof(x))-1) > 0 || ((typeof(divisor))-1) > 0 ||		\
	    (((__lkpi_x) > 0) == ((__lkpi_d) > 0))) ?			\
	    (((__lkpi_x) + ((__lkpi_d) / 2)) / (__lkpi_d)) :		\
	    (((__lkpi_x) - ((__lkpi_d) / 2)) / (__lkpi_d));		\
})

#define	DIV_ROUND_CLOSEST_ULL(x, divisor) ({				\
	typeof(divisor) __lkpi_dc = (divisor);				\
	unsigned long long __lkpi_xc = (x) + (__lkpi_dc) / 2;		\
	__lkpi_xc / __lkpi_dc;						\
})

/* Round x up/down to a multiple of y (any y > 0). */
#define	roundup(x, y)	({						\
	typeof(y) __lkpi_y = (y);					\
	(((x) + (__lkpi_y - 1)) / __lkpi_y) * __lkpi_y;			\
})

/* x * numer / denom without overflowing the intermediate x * numer. */
#define	mult_frac(x, numer, denom) ({					\
	typeof(x) __lkpi_q = (x) / (denom);				\
	typeof(x) __lkpi_r = (x) % (denom);				\
	(__lkpi_q * (numer)) + ((__lkpi_r * (numer)) / (denom));	\
})

/*
 * Type-generic absolute value. Replaces libc abs(); the result has the
 * argument's type (long for long, int for narrower types).
 */
#undef	abs
#define	abs(x)	__builtin_choose_expr(					\
	__builtin_types_compatible_p(typeof(x), long long) ||		\
	__builtin_types_compatible_p(typeof(x), long),			\
	({ long long __lkpi_a = (x); (typeof(x))(__lkpi_a < 0 ? -__lkpi_a : __lkpi_a); }), \
	({ int __lkpi_i = (x); __lkpi_i < 0 ? -__lkpi_i : __lkpi_i; }))

#define	abs_diff(a, b) ({						\
	typeof(a) __lkpi_ad_a = (a);					\
	typeof(b) __lkpi_ad_b = (b);					\
	__lkpi_ad_a > __lkpi_ad_b ? __lkpi_ad_a - __lkpi_ad_b :		\
	    __lkpi_ad_b - __lkpi_ad_a;					\
})

/* Map a 32-bit value uniformly into [0, ep_ro). */
static inline u32
reciprocal_scale(u32 val, u32 ep_ro)
{
	return ((u32)(((u64)val * ep_ro) >> 32));
}

/* floor(sqrt(x)), bit by bit. */
static inline unsigned long
int_sqrt(unsigned long x)
{
	unsigned long b, m, y = 0;

	if (x <= 1)
		return (x);
	m = 1UL << ((BITS_PER_LONG - 1 - __builtin_clzl(x)) & ~1UL);
	while (m != 0) {
		b = y + m;
		y >>= 1;
		if (x >= b) {
			x -= b;
			y += m;
		}
		m >>= 2;
	}
	return (y);
}

static inline u32
int_sqrt64(u64 x)
{
	return ((u32)int_sqrt((unsigned long)x));
}

#endif /* _LINUXKPI_LINUX_MATH_H_ */
