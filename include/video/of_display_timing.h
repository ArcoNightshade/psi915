/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Device-tree display timings: no device tree on x86
 * illumos; lookups fail with -ENOENT.
 */
#ifndef _LINUXKPI_VIDEO_OF_DISPLAY_TIMING_H_
#define	_LINUXKPI_VIDEO_OF_DISPLAY_TIMING_H_

#include <linux/types.h>

struct device_node;
struct display_timing;
struct display_timings;

#define	OF_USE_NATIVE_MODE	(-1)

#define	of_get_display_timing(np, name, dt)	\
	((void)(np), (void)(name), (void)(dt), -ENOENT)
#define	of_get_display_timings(np)	((void)(np), (struct display_timings *)NULL)

#endif /* _LINUXKPI_VIDEO_OF_DISPLAY_TIMING_H_ */
