/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. OOM notifiers: never called (illumos has no OOM killer callbacks).
 */
#ifndef _LINUXKPI_INCLUDE_LINUX_OOM_H_
#define	_LINUXKPI_INCLUDE_LINUX_OOM_H_

#include <linux/notifier.h>

#define	register_oom_notifier(nb)	((void)(nb), 0)
#define	unregister_oom_notifier(nb)	((void)(nb), 0)

#endif /* _LINUXKPI_INCLUDE_LINUX_OOM_H_ */
