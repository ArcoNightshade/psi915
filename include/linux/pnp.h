/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. ISA Plug and Play: not present; no range is reported as
 * reserved by a PnP device.
 */
#ifndef _LINUXKPI_LINUX_PNP_H_
#define	_LINUXKPI_LINUX_PNP_H_

#include <linux/types.h>

#define	pnp_range_reserved(start, end)	((void)(start), (void)(end), false)

#endif /* _LINUXKPI_LINUX_PNP_H_ */
