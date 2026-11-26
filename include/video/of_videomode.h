/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Device-tree video modes: no device tree; fails with
 * -ENOENT.
 */
#ifndef _LINUXKPI_VIDEO_OF_VIDEOMODE_H_
#define	_LINUXKPI_VIDEO_OF_VIDEOMODE_H_

#include <linux/types.h>

struct device_node;
struct videomode;

#define	of_get_videomode(np, vm, index)	\
	((void)(np), (void)(vm), (void)(index), -ENOENT)

#endif /* _LINUXKPI_VIDEO_OF_VIDEOMODE_H_ */
