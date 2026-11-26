/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. No address-space switching (kthread_use_mm is unsupported).
 */
#ifndef _LINUXKPI_INCLUDE_LINUX_MMU_CONTEXT_H_
#define	_LINUXKPI_INCLUDE_LINUX_MMU_CONTEXT_H_

#define	kthread_use_mm(mm)	do { (void)(mm); } while (0)
#define	kthread_unuse_mm(mm)	do { (void)(mm); } while (0)

#endif /* _LINUXKPI_INCLUDE_LINUX_MMU_CONTEXT_H_ */
