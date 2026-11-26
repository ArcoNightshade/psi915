/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. No device-memory cgroups.
 */
#ifndef _LINUXKPI_INCLUDE_LINUX_CGROUP_DMEM_H_
#define	_LINUXKPI_INCLUDE_LINUX_CGROUP_DMEM_H_

#include <linux/types.h>

struct dmem_cgroup_region;
struct dmem_cgroup_pool_state;
#define	dmem_cgroup_register_region(size, fmt, ...)	((struct dmem_cgroup_region *)NULL)
#define	dmem_cgroup_unregister_region(r)	do { (void)(r); } while (0)
#define	dmem_cgroup_try_charge(r, s, p, l)	(0)
#define	dmem_cgroup_uncharge(p, s)		do { (void)(p); } while (0)
#define	dmem_cgroup_pool_state_put(p)		do { (void)(p); } while (0)
#define	dmem_cgroup_state_evict_valuable(l, t, ig, ret)	(true)

#endif /* _LINUXKPI_INCLUDE_LINUX_CGROUP_DMEM_H_ */
