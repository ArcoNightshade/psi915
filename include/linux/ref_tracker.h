/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Reference trackers: disabled (no tracking, no reports).
 */
#ifndef _LINUXKPI_INCLUDE_LINUX_REF_TRACKER_H_
#define	_LINUXKPI_INCLUDE_LINUX_REF_TRACKER_H_

#include <linux/types.h>
#include <linux/gfp.h>

struct ref_tracker;
struct ref_tracker_dir {
	int	dummy;
};
#define	ref_tracker_dir_init(d, quarantine, name)	do { (void)(d); } while (0)
#define	ref_tracker_dir_exit(d)				do { (void)(d); } while (0)
#define	ref_tracker_dir_print(d, n)			do { (void)(d); } while (0)
#define	ref_tracker_dir_snprint(d, b, s)		(0)
#define	ref_tracker_alloc(d, t, g)			((void)(d), 0)
#define	ref_tracker_free(d, t)				((void)(d), 0)

#endif /* _LINUXKPI_INCLUDE_LINUX_REF_TRACKER_H_ */
