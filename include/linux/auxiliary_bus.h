/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Auxiliary bus: the KPI has no auxiliary bus driver model.
 * Devices can be initialised and released, but auxiliary_device_add()
 * fails with -ENODEV, so the parent driver treats the child as absent
 * (i915 only creates one for the GSC/MEI of DG1 and later, not on Kaby
 * Lake).
 */
#ifndef _LINUXKPI_LINUX_AUXILIARY_BUS_H_
#define	_LINUXKPI_LINUX_AUXILIARY_BUS_H_

#include <linux/device.h>
#include <linux/mod_devicetable.h>

struct auxiliary_device {
	struct device	dev;
	const char	*name;
	u32		id;
};

struct auxiliary_device_id {
	char		name[32];
	unsigned long	driver_data;
};

#define	to_auxiliary_dev(d)	container_of((d), struct auxiliary_device, dev)

static inline int
auxiliary_device_init(struct auxiliary_device *adev)
{
	if (adev->name == NULL || adev->dev.parent == NULL)
		return (-EINVAL);
	device_initialize(&adev->dev);
	return (0);
}

static inline int
__auxiliary_device_add(struct auxiliary_device *adev, const char *modname)
{
	(void) adev;
	(void) modname;
	return (-ENODEV);
}
#define	auxiliary_device_add(adev)	__auxiliary_device_add((adev), NULL)

static inline void
auxiliary_device_uninit(struct auxiliary_device *adev)
{
	put_device(&adev->dev);
}

static inline void
auxiliary_device_delete(struct auxiliary_device *adev)
{
	(void) adev;
}

#endif /* _LINUXKPI_LINUX_AUXILIARY_BUS_H_ */
