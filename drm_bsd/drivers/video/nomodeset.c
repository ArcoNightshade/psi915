/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 *
 * "nomodeset" policy for psi915: clean-room replacement for Linux drivers/video/nomodeset.c
 * (GPL-2.0).  Documented behaviour: video_firmware_drivers_only() is true when the administrator
 * asked that only firmware framebuffer drivers run, and DRM drivers then refuse to load
 * (drm_firmware_drivers_only() in the MIT <drm/drm_drv.h>).  Not derived from the Linux file.
 *
 * Linux sets this from "nomodeset" on the kernel command line.  On illumos the glue calls
 * video_bsd_set_firmware_drivers_only(), for example from a driver.conf property; the default
 * is false.
 */

#include <linux/compiler.h>
#include <linux/export.h>
#include <linux/types.h>

#include <video/nomodeset.h>

static bool video_bsd_nomodeset;

void
video_bsd_set_firmware_drivers_only(bool on)
{
	WRITE_ONCE(video_bsd_nomodeset, on);
}
EXPORT_SYMBOL(video_bsd_set_firmware_drivers_only);

bool
video_firmware_drivers_only(void)
{
	return (READ_ONCE(video_bsd_nomodeset));
}
EXPORT_SYMBOL(video_firmware_drivers_only);
