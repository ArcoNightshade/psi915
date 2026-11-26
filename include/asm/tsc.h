/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Time-stamp counter.
 */
#ifndef _LINUXKPI_INCLUDE_ASM_TSC_H_
#define	_LINUXKPI_INCLUDE_ASM_TSC_H_

#include <linux/types.h>

typedef unsigned long long cycles_t;

static inline cycles_t
rdtsc(void)
{
	u32 lo, hi;

	__asm__ __volatile__("rdtsc" : "=a" (lo), "=d" (hi));
	return (((cycles_t)hi << 32) | lo);
}
#define	get_cycles()	rdtsc()
#define	rdtsc_ordered()	rdtsc()

#endif /* _LINUXKPI_INCLUDE_ASM_TSC_H_ */
