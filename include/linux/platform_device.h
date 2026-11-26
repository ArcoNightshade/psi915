/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Platform devices: types only (none are created on illumos yet).
 */
#ifndef _LINUXKPI_INCLUDE_LINUX_PLATFORM_DEVICE_H_
#define	_LINUXKPI_INCLUDE_LINUX_PLATFORM_DEVICE_H_

#include <linux/device.h>
#include <linux/err.h>

struct platform_device {
	const char	*name;
	int		id;
	struct device	dev;
};

struct platform_driver {
	int		(*probe)(struct platform_device *);
	void		(*remove)(struct platform_device *);
	struct device_driver driver;
};

#define	to_platform_device(d)	container_of(d, struct platform_device, dev)
#define	platform_get_drvdata(p)	dev_get_drvdata(&(p)->dev)
#define	platform_set_drvdata(p, d) dev_set_drvdata(&(p)->dev, d)
#define	platform_device_register_full(i)	((struct platform_device *)ERR_PTR(-ENODEV))
#define	platform_device_unregister(p)		do { (void)(p); } while (0)
#define	platform_driver_register(d)		(0)
#define	platform_driver_unregister(d)		do { (void)(d); } while (0)

#endif /* _LINUXKPI_INCLUDE_LINUX_PLATFORM_DEVICE_H_ */
