/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Bus notifiers: the KPI buses never send BUS_NOTIFY_*
 * events (drivers are bound through DDI attach, not a Linux bus), so
 * registration succeeds and the callback is never called.
 */
#ifndef _LINUXKPI_LINUX_DEVICE_BUS_H_
#define	_LINUXKPI_LINUX_DEVICE_BUS_H_

#include <linux/device.h>
#include <linux/notifier.h>

#define	BUS_NOTIFY_ADD_DEVICE		0x00000001
#define	BUS_NOTIFY_DEL_DEVICE		0x00000002
#define	BUS_NOTIFY_REMOVED_DEVICE	0x00000003
#define	BUS_NOTIFY_BIND_DRIVER		0x00000004
#define	BUS_NOTIFY_BOUND_DRIVER		0x00000005
#define	BUS_NOTIFY_UNBIND_DRIVER	0x00000006
#define	BUS_NOTIFY_UNBOUND_DRIVER	0x00000007
#define	BUS_NOTIFY_DRIVER_NOT_BOUND	0x00000008

static inline int
bus_register_notifier(const struct bus_type *bus, struct notifier_block *nb)
{
	(void) bus;
	(void) nb;
	return (0);
}

static inline int
bus_unregister_notifier(const struct bus_type *bus, struct notifier_block *nb)
{
	(void) bus;
	(void) nb;
	return (0);
}

#endif /* _LINUXKPI_LINUX_DEVICE_BUS_H_ */
