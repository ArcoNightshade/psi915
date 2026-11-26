/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. MEI auxiliary device of the graphics GSC (DG1 and later;
 * never created by the KPI, see <linux/auxiliary_bus.h>).
 */
#ifndef _LINUXKPI_LINUX_MEI_AUX_H_
#define	_LINUXKPI_LINUX_MEI_AUX_H_

#include <linux/auxiliary_bus.h>
#include <linux/ioport.h>

struct mei_aux_device {
	struct auxiliary_device	aux_dev;
	int			irq;
	struct resource		bar;
	struct resource		ext_op_mem;
	bool			slow_firmware;
};

#define	auxiliary_dev_to_mei_aux_dev(a)	\
	container_of((a), struct mei_aux_device, aux_dev)

#endif /* _LINUXKPI_LINUX_MEI_AUX_H_ */
