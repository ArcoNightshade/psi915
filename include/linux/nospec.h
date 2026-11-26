/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Speculation-safe index clamping: array_index_nospec(i, n) is i if
 * i < n, else 0, computed without a branch the CPU could mispredict.
 */
#ifndef _LINUXKPI_LINUX_NOSPEC_H_
#define	_LINUXKPI_LINUX_NOSPEC_H_

#include <linux/types.h>
#include <linux/compiler.h>

static inline unsigned long
lkpi_array_index_mask_nospec(unsigned long index, unsigned long size)
{
	unsigned long mask;

	/* mask = (index < size) ? ~0 : 0, via the carry flag */
	__asm__ __volatile__("cmp %1, %2; sbb %0, %0" : "=r" (mask) :
	    "g" (size), "r" (index) : "cc");
	return (mask);
}

#define	array_index_nospec(index, size) ({				\
	typeof(index) __i = (index);					\
	typeof(size) __s = (size);					\
	(typeof(__i))(__i & lkpi_array_index_mask_nospec(		\
	    (unsigned long)__i, (unsigned long)__s));			\
})
#define	barrier_nospec()	__asm__ __volatile__("lfence" ::: "memory")

#endif /* _LINUXKPI_LINUX_NOSPEC_H_ */
