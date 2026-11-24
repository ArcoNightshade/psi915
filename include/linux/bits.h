/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation from the documented behaviour of the Linux
 * kernel API; macro set cross-checked against FreeBSD's LinuxKPI bits.h
 * (BSD-2-Clause).
 */
#ifndef _LINUXKPI_LINUX_BITS_H_
#define	_LINUXKPI_LINUX_BITS_H_

#include <linuxkpi/platform.h>
#include <linux/types.h>

#define	BITS_PER_BYTE		8
#define	BITS_PER_TYPE(t)	(sizeof(t) * BITS_PER_BYTE)

#define	BIT(nr)			(1UL << (nr))
#define	BIT_ULL(nr)		(1ULL << (nr))
#define	BIT_MASK(nr)		(1UL << ((nr) % BITS_PER_LONG))
#define	BIT_WORD(nr)		((nr) / BITS_PER_LONG)
#define	BIT_ULL_MASK(nr)	(1ULL << ((nr) % BITS_PER_LONG_LONG))
#define	BIT_ULL_WORD(nr)	((nr) / BITS_PER_LONG_LONG)
#define	BIT_U8(nr)		((u8)(1U << (nr)))
#define	BIT_U16(nr)		((u16)(1U << (nr)))
#define	BIT_U32(nr)		((u32)(1U << (nr)))
#define	BIT_U64(nr)		((u64)(1ULL << (nr)))

/* Contiguous mask from bit l to bit h inclusive (h >= l). */
#define	GENMASK(h, l)							\
	(((~0UL) - (1UL << (l)) + 1) & (~0UL >> (BITS_PER_LONG - 1 - (h))))
#define	GENMASK_ULL(h, l)						\
	(((~0ULL) - (1ULL << (l)) + 1) &				\
	    (~0ULL >> (BITS_PER_LONG_LONG - 1 - (h))))
#define	GENMASK_U8(h, l)	((u8)GENMASK(h, l))
#define	GENMASK_U16(h, l)	((u16)GENMASK(h, l))
#define	GENMASK_U32(h, l)	((u32)GENMASK(h, l))
#define	GENMASK_U64(h, l)	((u64)GENMASK_ULL(h, l))

#endif /* _LINUXKPI_LINUX_BITS_H_ */
