/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. mmap protection and flag constants (illumos values in the kernel).
 */
#ifndef _LINUXKPI_INCLUDE_LINUX_MMAN_H_
#define	_LINUXKPI_INCLUDE_LINUX_MMAN_H_

#include <linux/types.h>
#ifdef LINUXKPI_ILLUMOS_KERNEL
#include <sys/mman.h>
#else
#include <sys/mman.h>
#endif

#endif /* _LINUXKPI_INCLUDE_LINUX_MMAN_H_ */
