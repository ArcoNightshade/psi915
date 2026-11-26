/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. x86 CPU identification and feature bits, read with CPUID
 * once (src/linux_cpu.c). Feature numbers are word * 32 + bit, with
 * word 0 = CPUID.1:EDX, word 4 = CPUID.1:ECX, word 9 = CPUID.(7,0):EBX,
 * the layout Linux uses, so X86_FEATURE_* values match.
 */
#ifndef _LINUXKPI_ASM_CPUFEATURE_H_
#define	_LINUXKPI_ASM_CPUFEATURE_H_

#include <linux/types.h>

#define	LKPI_NCAPINTS		10

#define	X86_FEATURE_FPU		(0 * 32 + 0)
#define	X86_FEATURE_TSC		(0 * 32 + 4)
#define	X86_FEATURE_MSR		(0 * 32 + 5)
#define	X86_FEATURE_PAT		(0 * 32 + 16)
#define	X86_FEATURE_CLFLUSH	(0 * 32 + 19)
#define	X86_FEATURE_XMM		(0 * 32 + 25)
#define	X86_FEATURE_XMM2	(0 * 32 + 26)
#define	X86_FEATURE_SELFSNOOP	(0 * 32 + 27)
#define	X86_FEATURE_XMM3	(4 * 32 + 0)
#define	X86_FEATURE_SSSE3	(4 * 32 + 9)
#define	X86_FEATURE_XMM4_1	(4 * 32 + 19)
#define	X86_FEATURE_XMM4_2	(4 * 32 + 20)
#define	X86_FEATURE_AVX		(4 * 32 + 28)
#define	X86_FEATURE_HYPERVISOR	(4 * 32 + 31)
#define	X86_FEATURE_ERMS	(9 * 32 + 9)
#define	X86_FEATURE_SMAP	(9 * 32 + 20)
#define	X86_FEATURE_CLFLUSHOPT	(9 * 32 + 23)
#define	X86_FEATURE_CLWB	(9 * 32 + 24)

#define	X86_VENDOR_INTEL	0
#define	X86_VENDOR_AMD		2
#define	X86_VENDOR_UNKNOWN	0xff

struct cpuinfo_x86 {
	u8		x86;		/* family */
	u8		x86_vendor;
	u8		x86_model;
	u8		x86_stepping;
	u16		x86_clflush_size;
	int		x86_cache_alignment;
	u32		x86_capability[LKPI_NCAPINTS];
	char		x86_vendor_id[16];
	char		x86_model_id[64];
	u8		x86_phys_bits;
	u8		x86_virt_bits;
};

const struct cpuinfo_x86 *lkpi_boot_cpu_data(void);
#define	boot_cpu_data		(*lkpi_boot_cpu_data())

static inline bool
lkpi_cpu_has(unsigned int f)
{
	return ((boot_cpu_data.x86_capability[f / 32] >> (f % 32)) & 1);
}

#define	boot_cpu_has(f)		lkpi_cpu_has(f)
#define	static_cpu_has(f)	lkpi_cpu_has(f)
#define	cpu_feature_enabled(f)	lkpi_cpu_has(f)
#define	this_cpu_has(f)		lkpi_cpu_has(f)
#define	cpu_has(c, f)		lkpi_cpu_has(f)

#endif /* _LINUXKPI_ASM_CPUFEATURE_H_ */
