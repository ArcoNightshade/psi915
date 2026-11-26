/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. pfn_t with flags (only the plain pfn is used).
 */
#ifndef _LINUXKPI_INCLUDE_LINUX_PFN_T_H_
#define	_LINUXKPI_INCLUDE_LINUX_PFN_T_H_

#include <linux/types.h>
#include <linux/pgtable.h>

typedef struct {
	u64	val;
} pfn_t;
#define	PFN_DEV		(1ULL << 61)
#define	PFN_MAP		(1ULL << 60)
#define	__pfn_to_pfn_t(pfn, flags)	((pfn_t){ (u64)(pfn) | (flags) })
#define	pfn_t_to_pfn(p)			((unsigned long)((p).val & ~(PFN_DEV | PFN_MAP)))

#endif /* _LINUXKPI_INCLUDE_LINUX_PFN_T_H_ */
