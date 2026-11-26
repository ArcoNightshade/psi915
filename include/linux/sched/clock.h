/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Everything lives in <linux/sched.h>.
 */
#ifndef _LINUXKPI_LINUX_SCHED_CLOCK_H_
#define	_LINUXKPI_LINUX_SCHED_CLOCK_H_

#include <linux/sched.h>
#include <linux/ktime.h>

/* Nanosecond clocks (monotonic; no per-CPU clocks). */
#define	local_clock()		ktime_get_ns()
#define	sched_clock()		ktime_get_ns()
#define	cpu_clock(cpu)		((void)(cpu), ktime_get_ns())

#endif /* _LINUXKPI_LINUX_SCHED_CLOCK_H_ */
