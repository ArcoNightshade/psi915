/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. See <linux/preempt.h>.
 */
#ifndef _LINUXKPI_LINUX_HARDIRQ_H_
#define	_LINUXKPI_LINUX_HARDIRQ_H_

#include <linux/preempt.h>

#define	synchronize_hardirq(irq)	synchronize_irq(irq)

#endif /* _LINUXKPI_LINUX_HARDIRQ_H_ */
