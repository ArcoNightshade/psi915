/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. stop_machine(fn, data, cpus) runs fn(data) on the calling CPU without
 * stopping the others; the only DRM/i915 user (GGTT updates on old
 * chipsets with the VT-d workaround) is not used on Gen9.
 */
#ifndef _LINUXKPI_LINUX_STOP_MACHINE_H_
#define	_LINUXKPI_LINUX_STOP_MACHINE_H_

#include <linux/types.h>

struct cpumask;
#define	stop_machine(fn, data, cpus)	((fn)(data))
#define	stop_machine_cpuslocked(fn, data, cpus)	((fn)(data))

#endif /* _LINUXKPI_LINUX_STOP_MACHINE_H_ */
