/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. VGA register constants.
 */
#ifndef _LINUXKPI_INCLUDE_VIDEO_VGA_H_
#define	_LINUXKPI_INCLUDE_VIDEO_VGA_H_

#include <asm/io.h>	/* outb/inb, as Linux <video/vga.h> */

#define	VGA_FB_PHYS_BASE	0xA0000
#define	VGA_FB_PHYS_SIZE	65536
#define	VGA_MIS_W	0x3C2
#define	VGA_MIS_R	0x3CC
#define	VGA_SEQ_I	0x3C4
#define	VGA_SEQ_D	0x3C5
#define	VGA_SR01_SCREEN_OFF	0x20
#define	VGA_MIS_COLOR	0x01

#endif /* _LINUXKPI_INCLUDE_VIDEO_VGA_H_ */
