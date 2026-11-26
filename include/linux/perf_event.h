/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. perf events are not supported (CONFIG_PERF_EVENTS is off); the
 * types exist so i915_pmu.h compiles.
 */
#ifndef _LINUXKPI_INCLUDE_LINUX_PERF_EVENT_H_
#define	_LINUXKPI_INCLUDE_LINUX_PERF_EVENT_H_

#include <linux/types.h>
#include <linux/list.h>
#include <linux/sysfs.h>

struct lkpi_module;
struct perf_event;

struct pmu {
	struct lkpi_module		*module;
	const struct attribute_group	**attr_groups;
	const char			*name;
	int				type;
	int				capabilities;
	int				task_ctx_nr;
	int				(*event_init)(struct perf_event *event);
	int				(*add)(struct perf_event *event, int flags);
	void				(*del)(struct perf_event *event, int flags);
	void				(*start)(struct perf_event *event, int flags);
	void				(*stop)(struct perf_event *event, int flags);
	void				(*read)(struct perf_event *event);
	int				(*event_idx)(struct perf_event *event);
};

#define	perf_pmu_register(pmu, name, type)	(-ENODEV)
#define	perf_pmu_unregister(pmu)		do { } while (0)

#endif /* _LINUXKPI_INCLUDE_LINUX_PERF_EVENT_H_ */
