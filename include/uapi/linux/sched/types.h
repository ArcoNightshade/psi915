/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. struct sched_attr (scheduling is never changed).
 */
#ifndef _LINUXKPI_UAPI_LINUX_SCHED_TYPES_H_
#define	_LINUXKPI_UAPI_LINUX_SCHED_TYPES_H_

#include <linux/types.h>

struct sched_attr {
	u32	size;
	u32	sched_policy;
	u64	sched_flags;
	s32	sched_nice;
	u32	sched_priority;
	u64	sched_runtime;
	u64	sched_deadline;
	u64	sched_period;
};

#endif /* _LINUXKPI_UAPI_LINUX_SCHED_TYPES_H_ */
