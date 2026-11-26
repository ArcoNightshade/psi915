/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Device power-management callbacks. The PCI glue maps
 * illumos DDI_SUSPEND to prepare/suspend/suspend_late/suspend_noirq and
 * DDI_RESUME to resume_noirq/resume_early/resume/complete
 * (src/linux_pci.c); hibernation callbacks are never called (illumos has
 * no hibernation). Runtime PM: <linux/pm_runtime.h>.
 */
#ifndef _LINUXKPI_LINUX_PM_H_
#define	_LINUXKPI_LINUX_PM_H_

#include <linux/types.h>
#include <linux/atomic.h>

struct device;

typedef struct pm_message {
	int	event;
} pm_message_t;

#define	PM_EVENT_ON		0x0000
#define	PM_EVENT_FREEZE		0x0001
#define	PM_EVENT_SUSPEND	0x0002
#define	PM_EVENT_HIBERNATE	0x0004
#define	PM_EVENT_QUIESCE	0x0008
#define	PM_EVENT_RESUME		0x0010
#define	PMSG_SUSPEND		((struct pm_message){ .event = PM_EVENT_SUSPEND })
#define	PMSG_FREEZE		((struct pm_message){ .event = PM_EVENT_FREEZE })
#define	PMSG_ON			((struct pm_message){ .event = PM_EVENT_ON })

struct dev_pm_ops {
	int	(*prepare)(struct device *dev);
	void	(*complete)(struct device *dev);
	int	(*suspend)(struct device *dev);
	int	(*resume)(struct device *dev);
	int	(*freeze)(struct device *dev);
	int	(*thaw)(struct device *dev);
	int	(*poweroff)(struct device *dev);
	int	(*restore)(struct device *dev);
	int	(*suspend_late)(struct device *dev);
	int	(*resume_early)(struct device *dev);
	int	(*freeze_late)(struct device *dev);
	int	(*thaw_early)(struct device *dev);
	int	(*poweroff_late)(struct device *dev);
	int	(*restore_early)(struct device *dev);
	int	(*suspend_noirq)(struct device *dev);
	int	(*resume_noirq)(struct device *dev);
	int	(*freeze_noirq)(struct device *dev);
	int	(*thaw_noirq)(struct device *dev);
	int	(*poweroff_noirq)(struct device *dev);
	int	(*restore_noirq)(struct device *dev);
	int	(*runtime_suspend)(struct device *dev);
	int	(*runtime_resume)(struct device *dev);
	int	(*runtime_idle)(struct device *dev);
};

struct dev_pm_info {
	pm_message_t	power_state;
	bool		is_suspended;
	atomic_t	usage_count;
	bool		runtime_enabled;
};

#define	SET_SYSTEM_SLEEP_PM_OPS(s, r)					\
	.suspend = (s), .resume = (r), .freeze = (s), .thaw = (r),	\
	.poweroff = (s), .restore = (r),
#define	SET_LATE_SYSTEM_SLEEP_PM_OPS(s, r)				\
	.suspend_late = (s), .resume_early = (r),
#define	SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(s, r)				\
	.suspend_noirq = (s), .resume_noirq = (r),
#define	SET_RUNTIME_PM_OPS(s, r, i)					\
	.runtime_suspend = (s), .runtime_resume = (r), .runtime_idle = (i),
#define	SYSTEM_SLEEP_PM_OPS(s, r)	SET_SYSTEM_SLEEP_PM_OPS(s, r)
#define	RUNTIME_PM_OPS(s, r, i)		SET_RUNTIME_PM_OPS(s, r, i)
#define	pm_sleep_ptr(p)			(p)
#define	pm_ptr(p)			(p)
#define	DEFINE_SIMPLE_DEV_PM_OPS(name, s, r)				\
	const struct dev_pm_ops name = { SET_SYSTEM_SLEEP_PM_OPS(s, r) }

#define	device_set_wakeup_capable(dev, c)	do { } while (0)
#define	device_wakeup_enable(dev)		(0)
#define	device_may_wakeup(dev)			(false)
#define	pm_suspend_via_firmware()		(true)
#define	pm_resume_via_firmware()		(true)
#define	pm_suspend_target_state			(0)

#endif /* _LINUXKPI_LINUX_PM_H_ */
