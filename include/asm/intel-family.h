/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Intel CPU family/model identifiers (VFM: vendor, family,
 * model), only those the imported sources match on. Values are the CPUID
 * family 6 model numbers.
 */
#ifndef _LINUXKPI_ASM_INTEL_FAMILY_H_
#define	_LINUXKPI_ASM_INTEL_FAMILY_H_

#include <linux/types.h>
#include <asm/cpufeature.h>

#define	LKPI_VFM(v, f, m)	(((u32)(v) << 16) | ((u32)(f) << 8) | (u32)(m))
#define	LKPI_IFM(m)		LKPI_VFM(X86_VENDOR_INTEL, 6, (m))
#define	VFM_VENDOR(vfm)		(((vfm) >> 16) & 0xff)
#define	VFM_FAMILY(vfm)		(((vfm) >> 8) & 0xff)
#define	VFM_MODEL(vfm)		((vfm) & 0xff)

#define	INTEL_SKYLAKE_L		LKPI_IFM(0x4E)
#define	INTEL_SKYLAKE		LKPI_IFM(0x5E)
#define	INTEL_KABYLAKE_L	LKPI_IFM(0x8E)
#define	INTEL_KABYLAKE		LKPI_IFM(0x9E)
#define	INTEL_COMETLAKE		LKPI_IFM(0xA5)
#define	INTEL_COMETLAKE_L	LKPI_IFM(0xA6)
#define	INTEL_ROCKETLAKE	LKPI_IFM(0xA7)
#define	INTEL_ALDERLAKE		LKPI_IFM(0x97)
#define	INTEL_ALDERLAKE_L	LKPI_IFM(0x9A)
#define	INTEL_RAPTORLAKE	LKPI_IFM(0xB7)
#define	INTEL_RAPTORLAKE_P	LKPI_IFM(0xBA)
#define	INTEL_RAPTORLAKE_S	LKPI_IFM(0xBF)

#endif /* _LINUXKPI_ASM_INTEL_FAMILY_H_ */
