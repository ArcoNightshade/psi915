/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Runtime PM is not implemented: devices stay powered while
 * attached (docs/04, P7). The calls keep a usage count for debugging and
 * report "already active": pm_runtime_get_sync() returns 1,
 * pm_runtime_get_if_in_use()/_active() return 1, suspend requests do
 * nothing. The runtime_suspend/resume callbacks are never invoked.
 */
#ifndef _LINUXKPI_LINUX_PM_RUNTIME_H_
#define	_LINUXKPI_LINUX_PM_RUNTIME_H_

#include <linux/device.h>
#include <linux/pm.h>

static inline int
pm_runtime_get_sync(struct device *dev)
{
	atomic_inc(&dev->power.usage_count);
	return (1);
}

static inline int
pm_runtime_resume_and_get(struct device *dev)
{
	atomic_inc(&dev->power.usage_count);
	return (0);
}

static inline int
pm_runtime_put(struct device *dev)
{
	atomic_dec(&dev->power.usage_count);
	return (0);
}

static inline void
pm_runtime_get_noresume(struct device *dev)
{
	atomic_inc(&dev->power.usage_count);
}

#define	pm_runtime_get(dev)		pm_runtime_get_sync(dev)
#define	pm_runtime_put_sync(dev)	pm_runtime_put(dev)
#define	pm_runtime_put_autosuspend(dev)	pm_runtime_put(dev)
#define	pm_runtime_put_sync_suspend(dev) pm_runtime_put(dev)
#define	pm_runtime_put_noidle(dev)	((void)pm_runtime_put(dev))
#define	pm_runtime_get_if_in_use(dev)	(atomic_inc(&(dev)->power.usage_count), 1)
#define	pm_runtime_get_if_active(dev, ...) (atomic_inc(&(dev)->power.usage_count), 1)
#define	pm_runtime_enable(dev)		((void)((dev)->power.runtime_enabled = true))
#define	pm_runtime_disable(dev)		((void)((dev)->power.runtime_enabled = false))
#define	pm_runtime_allow(dev)		do { (void)(dev); } while (0)
#define	pm_runtime_forbid(dev)		do { (void)(dev); } while (0)
#define	pm_runtime_set_active(dev)	((void)(dev), 0)
#define	pm_runtime_set_suspended(dev)	((void)(dev), 0)
#define	pm_runtime_mark_last_busy(dev)	do { (void)(dev); } while (0)
#define	pm_runtime_use_autosuspend(dev)	do { (void)(dev); } while (0)
#define	pm_runtime_dont_use_autosuspend(dev) do { (void)(dev); } while (0)
#define	pm_runtime_set_autosuspend_delay(dev, d) do { (void)(dev); } while (0)
#define	pm_runtime_irq_safe(dev)	do { (void)(dev); } while (0)
#define	pm_runtime_suspended(dev)	((void)(dev), false)
#define	pm_runtime_active(dev)		((void)(dev), true)
#define	pm_runtime_status_suspended(dev) ((void)(dev), false)
#define	pm_runtime_enabled(dev)		((dev)->power.runtime_enabled)
#define	pm_runtime_idle(dev)		((void)(dev), 0)
#define	pm_runtime_suspend(dev)		((void)(dev), 0)
#define	pm_runtime_resume(dev)		((void)(dev), 0)
#define	pm_runtime_barrier(dev)		do { (void)(dev); } while (0)
#define	devm_pm_runtime_enable(dev)	((void)(dev), 0)

#endif /* _LINUXKPI_LINUX_PM_RUNTIME_H_ */
