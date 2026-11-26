/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. No device tree on illumos x86.
 */
#ifndef _LINUXKPI_LINUX_OF_H_
#define	_LINUXKPI_LINUX_OF_H_

#include <linux/types.h>
#include <linux/errno.h>

struct device_node;
#define	of_property_read_u32(n, p, v)	((void)(n), -ENODEV)
#define	of_node_put(n)			do { (void)(n); } while (0)
#define	of_node_get(n)			(n)
#define	of_match_ptr(p)			NULL

#endif /* _LINUXKPI_LINUX_OF_H_ */
