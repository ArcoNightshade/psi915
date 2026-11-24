/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * LINUXKPI_VERSION is the Linux kernel API level this KPI aims to provide,
 * encoded like FreeBSD's LinuxKPI: major * 10000 + minor * 100 + patch.
 * 61400 matches the drm-kmod snapshot recorded in docs/01 (Linux 6.14 base).
 * It will be revised when the Linux tag is pinned (docs/06, open question 4).
 *
 * LINUXKPI_RELEASE is the release of this project itself.
 */
#ifndef _LINUXKPI_VERSION_H_
#define	_LINUXKPI_VERSION_H_

#ifndef LINUXKPI_VERSION
#define	LINUXKPI_VERSION	61400
#endif

#define	LINUXKPI_RELEASE	"0.0.1-p0"

#endif /* _LINUXKPI_VERSION_H_ */
