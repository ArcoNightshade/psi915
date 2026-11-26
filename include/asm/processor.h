/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. See <asm/cpufeature.h>.
 */
#ifndef _LINUXKPI_ASM_PROCESSOR_H_
#define	_LINUXKPI_ASM_PROCESSOR_H_

#include <linuxkpi/platform.h>
#include <asm/cpufeature.h>

#define	cpu_relax()		lkpi_cpu_relax()
#define	INTEL_FAM6_KABYLAKE_L	0x8E
#define	INTEL_FAM6_KABYLAKE	0x9E

#endif /* _LINUXKPI_ASM_PROCESSOR_H_ */
