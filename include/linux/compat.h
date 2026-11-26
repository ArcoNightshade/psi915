/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. No 32-bit compat ioctls (32-bit callers are refused, docs/09 5.9).
 */
#ifndef _LINUXKPI_INCLUDE_LINUX_COMPAT_H_
#define	_LINUXKPI_INCLUDE_LINUX_COMPAT_H_

#include <linux/types.h>

#define	compat_ptr_ioctl	NULL
#define	in_compat_syscall()	(false)
#define	compat_ptr(p)		((void __user *)(uintptr_t)(p))
typedef u32 compat_uptr_t;
typedef u32 compat_ulong_t;

#endif /* _LINUXKPI_INCLUDE_LINUX_COMPAT_H_ */
