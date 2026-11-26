/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. System sleep state queries (illumos suspend goes through DDI_SUSPEND).
 */
#ifndef _LINUXKPI_INCLUDE_LINUX_SUSPEND_H_
#define	_LINUXKPI_INCLUDE_LINUX_SUSPEND_H_

#include <linux/types.h>
#include <linux/notifier.h>

typedef int suspend_state_t;
#define	PM_SUSPEND_ON		0
#define	PM_SUSPEND_TO_IDLE	1
#define	PM_SUSPEND_MEM		3
#define	pm_suspend_target_state	PM_SUSPEND_ON
#define	pm_suspend_default_s2idle()	(false)
#define	register_pm_notifier(nb)	((void)(nb), 0)
#define	unregister_pm_notifier(nb)	((void)(nb), 0)
#define	PM_HIBERNATION_PREPARE	0x0001
#define	PM_POST_HIBERNATION	0x0002
#define	PM_SUSPEND_PREPARE	0x0003
#define	PM_POST_SUSPEND		0x0004

#endif /* _LINUXKPI_INCLUDE_LINUX_SUSPEND_H_ */
