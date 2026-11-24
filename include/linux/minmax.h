/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation from the documented behaviour of the Linux
 * kernel API (linux/minmax.h).
 *
 * illumos <sys/ddi.h> defines min()/max() as plain macros; they are
 * replaced here. MIN()/MAX() are left to the environment (illumos
 * <sys/sysmacros.h>, libc <sys/param.h>), which defines them compatibly.
 */
#ifndef _LINUXKPI_LINUX_MINMAX_H_
#define	_LINUXKPI_LINUX_MINMAX_H_

#include <linuxkpi/platform.h>
#include <linux/compiler.h>
#include <linux/types.h>

/*
 * Type-checked min/max: both arguments are evaluated exactly once and must
 * have compatible types (the pointer comparison warns otherwise).
 */
#define	__lkpi_cmp(op, x, y, ux, uy) ({					\
	typeof(x) ux = (x);						\
	typeof(y) uy = (y);						\
	(void)(&ux == &uy);						\
	ux op uy ? ux : uy;						\
})

#undef	min
#undef	max
#define	min(x, y)	__lkpi_cmp(<, x, y, __UNIQUE_ID(min_x), __UNIQUE_ID(min_y))
#define	max(x, y)	__lkpi_cmp(>, x, y, __UNIQUE_ID(max_x), __UNIQUE_ID(max_y))
#define	min3(x, y, z)	min((typeof(x))min(x, y), z)
#define	max3(x, y, z)	max((typeof(x))max(x, y), z)

#define	__lkpi_cmp_t(t, op, x, y, ux, uy) ({				\
	t ux = (x);							\
	t uy = (y);							\
	ux op uy ? ux : uy;						\
})

#define	min_t(t, x, y)	\
	__lkpi_cmp_t(t, <, x, y, __UNIQUE_ID(mint_x), __UNIQUE_ID(mint_y))
#define	max_t(t, x, y)	\
	__lkpi_cmp_t(t, >, x, y, __UNIQUE_ID(maxt_x), __UNIQUE_ID(maxt_y))

#define	clamp(v, lo, hi)	min((typeof(v))max(v, lo), hi)
#define	clamp_t(t, v, lo, hi)	min_t(t, max_t(t, v, lo), hi)
#define	clamp_val(v, lo, hi)	clamp_t(typeof(v), v, lo, hi)

#define	__lkpi_swap(a, b, ut) do {					\
	typeof(a) ut = (a);						\
	(a) = (b);							\
	(b) = ut;							\
} while (0)
#define	swap(a, b)	__lkpi_swap(a, b, __UNIQUE_ID(swap_tmp))

/* Smaller of x and y, treating 0 as "unset" (returns the other one). */
#define	min_not_zero(x, y) ({						\
	typeof(x) __lkpi_mnz_x = (x);					\
	typeof(y) __lkpi_mnz_y = (y);					\
	__lkpi_mnz_x == 0 ? __lkpi_mnz_y :				\
	    (__lkpi_mnz_y == 0 ? __lkpi_mnz_x : min(__lkpi_mnz_x, __lkpi_mnz_y)); \
})

/*
 * Unsigned min/max: operands are promoted through unsigned int, unsigned
 * long and unsigned long long, so umax(-1, 5) is UINT_MAX, as in Linux.
 */
#define	umin(x, y)							\
	min((x) + 0u + 0ul + 0ull, (y) + 0u + 0ul + 0ull)
#define	umax(x, y)							\
	max((x) + 0u + 0ul + 0ull, (y) + 0u + 0ul + 0ull)

/* Whether val lies in [start, start + len), in unsigned arithmetic. */
#define	in_range32(val, start, len)					\
	((u32)((u32)(val) - (u32)(start)) < (u32)(len))
#define	in_range64(val, start, len)					\
	((u64)((u64)(val) - (u64)(start)) < (u64)(len))
#define	in_range(val, start, len)					\
	((sizeof(start) | sizeof(len) | sizeof(val)) <= sizeof(u32) ?	\
	    in_range32(val, start, len) : in_range64(val, start, len))

#endif /* _LINUXKPI_LINUX_MINMAX_H_ */
