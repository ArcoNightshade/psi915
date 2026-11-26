/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. GPIO lookup tables (board files). The KPI has no GPIO
 * controllers, so tables are accepted and never consulted; gpiod_get()
 * fails with -ENOENT (<linux/gpio/consumer.h>).
 */
#ifndef _LINUXKPI_LINUX_GPIO_MACHINE_H_
#define	_LINUXKPI_LINUX_GPIO_MACHINE_H_

#include <linux/types.h>

enum gpio_lookup_flags {
	GPIO_ACTIVE_HIGH = 0,
	GPIO_ACTIVE_LOW = 1,
	GPIO_OPEN_DRAIN = 2,
	GPIO_OPEN_SOURCE = 4,
	GPIO_PERSISTENT = 0,
	GPIO_TRANSITORY = 8,
	GPIO_PULL_UP = 16,
	GPIO_PULL_DOWN = 32,
	GPIO_PULL_DISABLE = 64,
	GPIO_LOOKUP_FLAGS_DEFAULT = 0
};

struct gpiod_lookup {
	const char	*key;
	u16		chip_hwnum;
	const char	*con_id;
	unsigned int	idx;
	unsigned long	flags;
};

struct gpiod_lookup_table {
	struct list_head	list;
	const char		*dev_id;
	struct gpiod_lookup	table[];
};

#define	GPIO_LOOKUP_IDX(_key, _hw, _con, _idx, _flags)			\
	(struct gpiod_lookup) { .key = (_key), .chip_hwnum = (_hw),	\
	    .con_id = (_con), .idx = (_idx), .flags = (_flags) }
#define	GPIO_LOOKUP(_key, _hw, _con, _flags)				\
	GPIO_LOOKUP_IDX(_key, _hw, _con, 0, _flags)

#define	gpiod_add_lookup_table(t)	do { (void)(t); } while (0)
#define	gpiod_remove_lookup_table(t)	do { (void)(t); } while (0)

#endif /* _LINUXKPI_LINUX_GPIO_MACHINE_H_ */
