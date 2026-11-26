/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Model-specific registers via illumos rdmsr()/wrmsr()
 * (<sys/x86_archext.h>); read as 0 in userland tests. The _safe forms
 * cannot catch #GP on illumos and are only correct for MSRs that exist.
 */
#ifndef _LINUXKPI_ASM_MSR_H_
#define	_LINUXKPI_ASM_MSR_H_

#include <linuxkpi/plat_dev.h>

#define	rdmsrl(msr, val)	((val) = lkpi_plat_rdmsr(msr))
#define	wrmsrl(msr, val)	lkpi_plat_wrmsr(msr, val)
#define	rdmsrl_safe(msr, p)	(*(p) = lkpi_plat_rdmsr(msr), 0)
#define	rdmsr(msr, lo, hi) do {						\
	u64 __v = lkpi_plat_rdmsr(msr);					\
	(lo) = (u32)__v;						\
	(hi) = (u32)(__v >> 32);					\
} while (0)

#endif /* _LINUXKPI_ASM_MSR_H_ */
