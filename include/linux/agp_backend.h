/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. AGP memory types, as used by the gen2-5 GMCH GTT code
 * (not used on Kaby Lake).
 */
#ifndef _LINUXKPI_LINUX_AGP_BACKEND_H_
#define	_LINUXKPI_LINUX_AGP_BACKEND_H_

#define	AGP_NORMAL_MEMORY	0
#define	AGP_USER_TYPES		(1 << 16)
#define	AGP_USER_MEMORY		(AGP_USER_TYPES)
#define	AGP_USER_CACHED_MEMORY	(AGP_USER_TYPES + 1)

#endif /* _LINUXKPI_LINUX_AGP_BACKEND_H_ */
