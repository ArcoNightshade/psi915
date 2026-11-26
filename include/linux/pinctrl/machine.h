/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Pin control board mappings: accepted, never applied.
 */
#ifndef _LINUXKPI_LINUX_PINCTRL_MACHINE_H_
#define	_LINUXKPI_LINUX_PINCTRL_MACHINE_H_

#include <linux/types.h>

enum pinctrl_map_type {
	PIN_MAP_TYPE_INVALID,
	PIN_MAP_TYPE_DUMMY_STATE,
	PIN_MAP_TYPE_MUX_GROUP,
	PIN_MAP_TYPE_CONFIGS_PIN,
	PIN_MAP_TYPE_CONFIGS_GROUP
};

struct pinctrl_map_mux {
	const char	*group;
	const char	*function;
};

struct pinctrl_map {
	const char		*dev_name;
	const char		*name;
	enum pinctrl_map_type	type;
	const char		*ctrl_dev_name;
	union {
		struct pinctrl_map_mux	mux;
	} data;
};

#define	PIN_MAP_MUX_GROUP(dev, state, pinctrl, grp, func)		\
	{ .dev_name = (dev), .name = (state),				\
	  .type = PIN_MAP_TYPE_MUX_GROUP, .ctrl_dev_name = (pinctrl),	\
	  .data.mux = { .group = (grp), .function = (func) } }

#define	pinctrl_register_mappings(m, n)	((void)(m), (void)(n), 0)
#define	pinctrl_unregister_mappings(m)	do { (void)(m); } while (0)

#endif /* _LINUXKPI_LINUX_PINCTRL_MACHINE_H_ */
