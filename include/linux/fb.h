/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. fbdev: only the constants DRM headers use (fbdev emulation is off).
 */
#ifndef _LINUXKPI_INCLUDE_LINUX_FB_H_
#define	_LINUXKPI_INCLUDE_LINUX_FB_H_

#include <linux/types.h>
#include <linux/fs.h>
#include <linux/seq_file.h>

struct fb_info;
#define	FBINFO_STATE_RUNNING	0
#define	FBINFO_STATE_SUSPENDED	1
#define	FB_BLANK_UNBLANK	0
#define	FB_BLANK_NORMAL		1
#define	FB_BLANK_VSYNC_SUSPEND	2
#define	FB_BLANK_HSYNC_SUSPEND	3
#define	FB_BLANK_POWERDOWN	4

#define	KHZ2PICOS(a)		(1000000000UL / (a))
#define	PICOS2KHZ(a)		(1000000000UL / (a))

#endif /* _LINUXKPI_INCLUDE_LINUX_FB_H_ */
