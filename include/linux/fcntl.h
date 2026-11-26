/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Open flags with the ENVIRONMENT's values: illumos
 * <sys/fcntl.h> in the kernel (so DRM_CLOEXEC = O_CLOEXEC = 0x800000
 * matches what illumos libdrm passes, docs/09 5.2), the host's
 * <fcntl.h> in userland tests. Linux code must never hard-code the
 * numeric Linux values.
 */
#ifndef _LINUXKPI_LINUX_FCNTL_H_
#define	_LINUXKPI_LINUX_FCNTL_H_

#include <linuxkpi/platform.h>

#ifdef LINUXKPI_ILLUMOS_KERNEL
#include <sys/fcntl.h>
#else
#include <fcntl.h>
#endif

#ifndef O_CLOEXEC
#error "linuxkpi: O_CLOEXEC missing from the environment headers"
#endif
#ifndef O_LARGEFILE
#define	O_LARGEFILE	0
#endif
#ifndef O_NDELAY
#define	O_NDELAY	O_NONBLOCK
#endif

#endif /* _LINUXKPI_LINUX_FCNTL_H_ */
