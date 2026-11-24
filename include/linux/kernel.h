/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation from the documented behaviour of the Linux
 * kernel API. P0 slice: only side-effect-free helpers (array size, min/max,
 * rounding). printk, WARN/BUG, kstrto* etc. arrive in P1.
 */
#ifndef _LINUXKPI_LINUX_KERNEL_H_
#define	_LINUXKPI_LINUX_KERNEL_H_

#include <linuxkpi/platform.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/stddef.h>
#include <linux/build_bug.h>
#include <linux/container_of.h>

/*
 * illumos <sys/sysmacros.h> defines ARRAY_SIZE without the array check and
 * <sys/ddi.h> defines min()/max() as plain macros. Linux code expects the
 * stricter versions below, so replace them. KPI sources must include
 * linux/ headers after illumos DDI headers.
 */
#undef	ARRAY_SIZE
#define	ARRAY_SIZE(a)	(sizeof(a) / sizeof((a)[0]) + __must_be_array(a))

#define	U8_MAX		((u8)~0U)
#define	S8_MAX		((s8)(U8_MAX >> 1))
#define	S8_MIN		((s8)(-S8_MAX - 1))
#define	U16_MAX		((u16)~0U)
#define	S16_MAX		((s16)(U16_MAX >> 1))
#define	S16_MIN		((s16)(-S16_MAX - 1))
#define	U32_MAX		((u32)~0U)
#define	S32_MAX		((s32)(U32_MAX >> 1))
#define	S32_MIN		((s32)(-S32_MAX - 1))
#define	U64_MAX		((u64)~0ULL)
#define	S64_MAX		((s64)(U64_MAX >> 1))
#define	S64_MIN		((s64)(-S64_MAX - 1))

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

#define	DIV_ROUND_UP(n, d)	(((n) + (d) - 1) / (d))
#define	DIV_ROUND_UP_ULL(n, d)	\
	((unsigned long long)DIV_ROUND_UP((unsigned long long)(n), (d)))

/* Power-of-two alignment helpers (a must be a power of two). */
#define	__ALIGN_MASK(x, mask)	(((x) + (mask)) & ~(mask))
#undef	ALIGN
#define	ALIGN(x, a)		__ALIGN_MASK(x, (typeof(x))(a) - 1)
#define	ALIGN_DOWN(x, a)	ALIGN((x) - ((a) - 1), (a))
#define	IS_ALIGNED(x, a)	(((x) & ((typeof(x))(a) - 1)) == 0)
#define	PTR_ALIGN(p, a)		((typeof(p))ALIGN((uintptr_t)(p), (a)))

#define	__round_mask(x, y)	((typeof(x))((y) - 1))
#define	round_up(x, y)		((((x) - 1) | __round_mask(x, y)) + 1)
#define	round_down(x, y)	((x) & ~__round_mask(x, y))

/* Arbitrary (not necessarily power-of-two) rounding. */
#define	__lkpi_rounddown(x, y, ux)	({ typeof(x) ux = (x); ux - (ux % (y)); })
#define	rounddown(x, y)	__lkpi_rounddown(x, y, __UNIQUE_ID(rdown_x))

#define	upper_32_bits(n)	((u32)(((n) >> 16) >> 16))
#define	lower_32_bits(n)	((u32)((n) & 0xffffffff))

#endif /* _LINUXKPI_LINUX_KERNEL_H_ */
