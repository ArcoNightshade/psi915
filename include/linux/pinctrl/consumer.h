/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Pin control: no pin controllers (Bay Trail SoC PWM
 * muxing only); lookups fail with -ENODEV.
 */
#ifndef _LINUXKPI_LINUX_PINCTRL_CONSUMER_H_
#define	_LINUXKPI_LINUX_PINCTRL_CONSUMER_H_

#include <linux/err.h>
#include <linux/pinctrl/machine.h>

struct pinctrl;
struct device;

#define	pinctrl_get_select(dev, s)	\
	((void)(dev), (void)(s), (struct pinctrl *)ERR_PTR(-ENODEV))
#define	devm_pinctrl_get_select(dev, s)	pinctrl_get_select(dev, s)
#define	pinctrl_put(p)			do { (void)(p); } while (0)

#endif /* _LINUXKPI_LINUX_PINCTRL_CONSUMER_H_ */
