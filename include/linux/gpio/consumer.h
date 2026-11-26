/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. No GPIO framework (i915 uses it only for DSI panels).
 */
#ifndef _LINUXKPI_INCLUDE_LINUX_GPIO_CONSUMER_H_
#define	_LINUXKPI_INCLUDE_LINUX_GPIO_CONSUMER_H_

#include <linux/types.h>
#include <linux/err.h>

struct gpio_desc;
#define	GPIOD_OUT_LOW		0
#define	GPIOD_OUT_HIGH		1
#define	GPIOD_ASIS		2
#define	gpiod_get(d, n, f)		((struct gpio_desc *)ERR_PTR(-ENOENT))
#define	gpiod_get_index(d, n, i, f)	((struct gpio_desc *)ERR_PTR(-ENOENT))
#define	gpiod_put(g)			do { (void)(g); } while (0)
#define	gpiod_set_value_cansleep(g, v)	do { (void)(g); } while (0)
#define	gpiod_set_value(g, v)		do { (void)(g); } while (0)

#endif /* _LINUXKPI_INCLUDE_LINUX_GPIO_CONSUMER_H_ */
