/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Component framework (aggregate devices): not provided. component_add()
 * succeeds without binding anything, so i915's optional HDA audio and MEI
 * components stay unbound (as when those drivers are absent on Linux).
 */
#ifndef _LINUXKPI_LINUX_COMPONENT_H_
#define	_LINUXKPI_LINUX_COMPONENT_H_

#include <linux/device.h>

struct component_ops {
	int	(*bind)(struct device *comp, struct device *master, void *data);
	void	(*unbind)(struct device *comp, struct device *master, void *data);
};

struct component_master_ops {
	int	(*bind)(struct device *master);
	void	(*unbind)(struct device *master);
};

struct component_match;
#define	component_add(dev, ops)			((void)(dev), (void)(ops), 0)
#define	component_add_typed(dev, ops, t)	((void)(dev), (void)(ops), 0)
#define	component_del(dev, ops)			do { (void)(dev); } while (0)
#define	component_bind_all(m, d)		((void)(m), 0)
#define	component_unbind_all(m, d)		do { (void)(m); } while (0)
#define	component_master_add_with_match(d, o, m) (-ENODEV)
#define	component_master_del(d, o)		do { (void)(d); } while (0)
#define	component_match_add_typed(d, m, c, cd)	do { (void)(d); } while (0)

#endif /* _LINUXKPI_LINUX_COMPONENT_H_ */
