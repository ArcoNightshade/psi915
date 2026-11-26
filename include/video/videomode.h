/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Generic video timings (struct videomode, display_timing).
 */
#ifndef _LINUXKPI_VIDEO_VIDEOMODE_H_
#define	_LINUXKPI_VIDEO_VIDEOMODE_H_

#include <linux/types.h>
#include <linux/bits.h>

enum display_flags {
	DISPLAY_FLAGS_HSYNC_LOW		= BIT(0),
	DISPLAY_FLAGS_HSYNC_HIGH	= BIT(1),
	DISPLAY_FLAGS_VSYNC_LOW		= BIT(2),
	DISPLAY_FLAGS_VSYNC_HIGH	= BIT(3),
	DISPLAY_FLAGS_DE_LOW		= BIT(4),
	DISPLAY_FLAGS_DE_HIGH		= BIT(5),
	DISPLAY_FLAGS_PIXDATA_POSEDGE	= BIT(6),
	DISPLAY_FLAGS_PIXDATA_NEGEDGE	= BIT(7),
	DISPLAY_FLAGS_INTERLACED	= BIT(8),
	DISPLAY_FLAGS_DOUBLESCAN	= BIT(9),
	DISPLAY_FLAGS_DOUBLECLK		= BIT(10),
	DISPLAY_FLAGS_SYNC_POSEDGE	= BIT(11),
	DISPLAY_FLAGS_SYNC_NEGEDGE	= BIT(12)
};

struct videomode {
	unsigned long	pixelclock;
	u32		hactive;
	u32		hfront_porch;
	u32		hback_porch;
	u32		hsync_len;
	u32		vactive;
	u32		vfront_porch;
	u32		vback_porch;
	u32		vsync_len;
	enum display_flags flags;
};

#endif /* _LINUXKPI_VIDEO_VIDEOMODE_H_ */
