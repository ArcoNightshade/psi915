/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. CPU cache maintenance (src/linux_cpu.c):
 *
 *   clflush_cache_range()  CLFLUSHOPT (or CLFLUSH) every line of the
 *                          range, fenced with MFENCE on both sides; the
 *                          instructions are broadcast coherently, so this
 *                          is correct on every CPU
 *   wbinvd_on_all_cpus()   WBINVD on the calling CPU only (illumos has no
 *                          cheap all-CPU cross call for leaf drivers);
 *                          i915 uses it only as a fallback when CLFLUSH
 *                          is missing, which never applies to Gen9 CPUs
 */
#ifndef _LINUXKPI_ASM_CACHEFLUSH_H_
#define	_LINUXKPI_ASM_CACHEFLUSH_H_

#include <linux/types.h>
#include <asm/cpufeature.h>

void clflush_cache_range(void *addr, unsigned int size);
void wbinvd_on_all_cpus(void);

static inline void
clflush(volatile void *p)
{
	__asm__ __volatile__("clflush %0" : "+m" (*(volatile char *)p));
}

static inline void
clflushopt(volatile void *p)
{
	__asm__ __volatile__(".byte 0x66; clflush %0" :
	    "+m" (*(volatile char *)p));
}

#define	wbinvd()		wbinvd_on_all_cpus()
#define	flush_cache_all()	do { } while (0)

#endif /* _LINUXKPI_ASM_CACHEFLUSH_H_ */
