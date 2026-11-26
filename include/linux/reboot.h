/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Reboot notifiers: never called.
 */
#ifndef _LINUXKPI_INCLUDE_LINUX_REBOOT_H_
#define	_LINUXKPI_INCLUDE_LINUX_REBOOT_H_

#include <linux/notifier.h>

#define	SYS_DOWN	0x0001
#define	SYS_RESTART	SYS_DOWN
#define	SYS_HALT	0x0002
#define	SYS_POWER_OFF	0x0003
#define	register_reboot_notifier(nb)	((void)(nb), 0)
#define	unregister_reboot_notifier(nb)	((void)(nb), 0)

#endif /* _LINUXKPI_INCLUDE_LINUX_REBOOT_H_ */
