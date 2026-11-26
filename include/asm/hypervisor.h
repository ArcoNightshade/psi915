/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Hypervisor detection: CPUID.1:ECX bit 31 means "running under a
 * hypervisor"; the vendor is not identified (reported as generic).
 */
#ifndef _LINUXKPI_INCLUDE_ASM_HYPERVISOR_H_
#define	_LINUXKPI_INCLUDE_ASM_HYPERVISOR_H_

#include <linux/types.h>
#include <asm/cpufeature.h>

enum x86_hypervisor_type {
	X86_HYPER_NATIVE = 0,
	X86_HYPER_VMWARE,
	X86_HYPER_MS_HYPERV,
	X86_HYPER_XEN_PV,
	X86_HYPER_XEN_HVM,
	X86_HYPER_KVM,
	X86_HYPER_JAILHOUSE,
	X86_HYPER_ACRN
};

static inline bool
hypervisor_is_type(enum x86_hypervisor_type t)
{
	bool hv = boot_cpu_has(X86_FEATURE_HYPERVISOR);

	return (t == X86_HYPER_NATIVE ? !hv : false);
}

#endif /* _LINUXKPI_INCLUDE_ASM_HYPERVISOR_H_ */
