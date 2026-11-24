/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation from the documented behaviour of the Linux
 * kernel API. min/max live in <linux/minmax.h>, rounding helpers here and
 * in <linux/math.h>.
 */
#ifndef _LINUXKPI_LINUX_KERNEL_H_
#define	_LINUXKPI_LINUX_KERNEL_H_

#include <linuxkpi/platform.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/stddef.h>
#include <linux/build_bug.h>
#include <linux/container_of.h>
#include <linux/limits.h>
#include <linux/minmax.h>
#include <linux/math.h>
#include <linux/bitops.h>
#include <linux/log2.h>
#include <linux/printk.h>
#include <linux/bug.h>
#include <linux/panic.h>
#include <linux/err.h>
#include <linux/errno.h>

/*
 * illumos <sys/sysmacros.h> defines ARRAY_SIZE without the array check
 * (and <sys/ddi.h> plain min()/max(), see <linux/minmax.h>). Linux code
 * expects the stricter versions, so replace them. KPI sources must include
 * linux/ headers after illumos DDI headers.
 */
#undef	ARRAY_SIZE
#define	ARRAY_SIZE(a)	(sizeof(a) / sizeof((a)[0]) + __must_be_array(a))

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
