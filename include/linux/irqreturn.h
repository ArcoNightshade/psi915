/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room.
 */
#ifndef _LINUXKPI_LINUX_IRQRETURN_H_
#define	_LINUXKPI_LINUX_IRQRETURN_H_

enum irqreturn {
	IRQ_NONE = 0,
	IRQ_HANDLED = 1,
	IRQ_WAKE_THREAD = 2
};
typedef enum irqreturn irqreturn_t;
#define	IRQ_RETVAL(x)	((x) ? IRQ_HANDLED : IRQ_NONE)

#endif /* _LINUXKPI_LINUX_IRQRETURN_H_ */
