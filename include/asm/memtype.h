/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. x86 PAT: illumos programs the PAT (write-combining is
 * available through gfxp/hat attributes).
 */
#ifndef _LINUXKPI_ASM_MEMTYPE_H_
#define	_LINUXKPI_ASM_MEMTYPE_H_

#include <linux/types.h>

#define	pat_enabled()		(true)

#endif /* _LINUXKPI_ASM_MEMTYPE_H_ */
