/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Virtual terminals: none (the illumos console is not a
 * Linux VT); only the notifier constants.
 */
#ifndef _LINUXKPI_LINUX_VT_H_
#define	_LINUXKPI_LINUX_VT_H_

#define	VT_ALLOCATE		0x0001
#define	VT_DEALLOCATE		0x0002
#define	VT_WRITE		0x0003
#define	VT_UPDATE		0x0004
#define	VT_PREWRITE		0x0005

#endif /* _LINUXKPI_LINUX_VT_H_ */
