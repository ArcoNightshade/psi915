/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. No firmware node properties on illumos x86.
 */
#ifndef _LINUXKPI_INCLUDE_LINUX_PROPERTY_H_
#define	_LINUXKPI_INCLUDE_LINUX_PROPERTY_H_

#include <linux/types.h>
#include <linux/errno.h>

struct fwnode_operations;
struct device;

struct fwnode_handle {
	struct fwnode_handle		*secondary;
	const struct fwnode_operations	*ops;
	struct device			*dev;
};
#define	device_property_read_u32(d, p, v)	((void)(d), -ENODEV)
#define	device_property_present(d, p)		((void)(d), false)
#define	dev_fwnode(d)				((struct fwnode_handle *)NULL)

#endif /* _LINUXKPI_INCLUDE_LINUX_PROPERTY_H_ */
