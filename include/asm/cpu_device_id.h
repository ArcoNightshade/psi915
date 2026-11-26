/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. x86 CPU match tables: x86_match_cpu() compares the boot
 * CPU vendor, family and model (src/linux_cpu.c).
 */
#ifndef _LINUXKPI_ASM_CPU_DEVICE_ID_H_
#define	_LINUXKPI_ASM_CPU_DEVICE_ID_H_

#include <linux/types.h>
#include <asm/cpufeature.h>
#include <asm/intel-family.h>

struct x86_cpu_id {
	u16		vendor;
	u16		family;
	u16		model;
	u16		steppings;
	u16		feature;
	u16		flags;
	unsigned long	driver_data;
};

#define	X86_VENDOR_ANY		0xffff
#define	X86_FAMILY_ANY		0
#define	X86_MODEL_ANY		0
#define	X86_FEATURE_ANY		0
#define	X86_CPU_ID_FLAG_ENTRY_VALID	0x1

#define	X86_MATCH_VFM(vfm, data)	{				\
	.vendor = VFM_VENDOR(vfm), .family = VFM_FAMILY(vfm),		\
	.model = VFM_MODEL(vfm), .feature = X86_FEATURE_ANY,		\
	.flags = X86_CPU_ID_FLAG_ENTRY_VALID,				\
	.driver_data = (unsigned long)(data) }

const struct x86_cpu_id *x86_match_cpu(const struct x86_cpu_id *match);

#endif /* _LINUXKPI_ASM_CPU_DEVICE_ID_H_ */
