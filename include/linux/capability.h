/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Capabilities: capable(cap) asks the platform whether the current
 * credentials are privileged (illumos drv_priv(9F) on CRED(); euid 0 in
 * userland). Every capability maps to that single check.
 */
#ifndef _LINUXKPI_INCLUDE_LINUX_CAPABILITY_H_
#define	_LINUXKPI_INCLUDE_LINUX_CAPABILITY_H_

#include <linux/types.h>

#define	CAP_SYS_ADMIN		21
#define	CAP_SYS_NICE		23
#define	CAP_SYS_RAWIO		17
#define	CAP_PERFMON		38
bool lkpi_capable(int cap);
#define	capable(cap)		lkpi_capable(cap)
#define	perfmon_capable()	lkpi_capable(CAP_PERFMON)
#define	has_capability(t, c)	lkpi_capable(c)

#endif /* _LINUXKPI_INCLUDE_LINUX_CAPABILITY_H_ */
