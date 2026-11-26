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
#define	of_property_present(n, p)	((void)(n), (void)(p), false)
#define	of_parse_phandle(n, p, i)	\
	((void)(n), (void)(p), (void)(i), (struct device_node *)NULL)
#define	of_fwnode_handle(n)		((void)(n), (struct fwnode_handle *)NULL)
#define	of_driver_match_device(d, drv)	((void)(d), (void)(drv), 0)
#define	of_device_uevent_modalias(d, e)	((void)(d), (void)(e), -ENODEV)
#define	for_each_available_child_of_node(parent, child)		\
	for ((void)(parent), (child) = NULL; (child) != NULL; )
#define	for_each_child_of_node(parent, child)				\
	for_each_available_child_of_node(parent, child)

#endif /* _LINUXKPI_LINUX_OF_H_ */
