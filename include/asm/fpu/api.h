/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Kernel FPU/SIMD sections. Not supported on illumos yet:
 * the KPI hides SSE4.1 in the kernel (src/linux_cpu.c), so the only users
 * (MOVNTDQA copies in i915_memcpy.c and drm_cache.c) are never reached,
 * and kernel_fpu_begin() panics if called. In userland (tests) the
 * section is a no-op.
 */
#ifndef _LINUXKPI_ASM_FPU_API_H_
#define	_LINUXKPI_ASM_FPU_API_H_

#include <linux/types.h>

void lkpi_kernel_fpu_begin(void);
void lkpi_kernel_fpu_end(void);
#define	kernel_fpu_begin()	lkpi_kernel_fpu_begin()
#define	kernel_fpu_end()	lkpi_kernel_fpu_end()
#define	kernel_fpu_available()	(false)
#define	irq_fpu_usable()	(false)

#endif /* _LINUXKPI_ASM_FPU_API_H_ */
