/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. No ACPI video driver on illumos: the native backlight is used.
 */
#ifndef _LINUXKPI_INCLUDE_ACPI_VIDEO_H_
#define	_LINUXKPI_INCLUDE_ACPI_VIDEO_H_

#include <linux/types.h>
#include <linux/errno.h>

enum acpi_backlight_type {
	acpi_backlight_undef = -1,
	acpi_backlight_none = 0,
	acpi_backlight_video,
	acpi_backlight_vendor,
	acpi_backlight_native,
	acpi_backlight_nvidia_wmi_ec,
	acpi_backlight_apple_gmux,
	acpi_backlight_dell_uart
};

#define	ACPI_VIDEO_CLASS		"video"
#define	ACPI_VIDEO_NOTIFY_PROBE		0x81
#define	acpi_video_register()		(0)
#define	acpi_video_unregister()		do { } while (0)
#define	acpi_video_register_backlight()	do { } while (0)
#define	acpi_video_get_backlight_type()	acpi_backlight_native
#define	acpi_video_backlight_use_native() (true)
#define	acpi_video_report_nolcd()	do { } while (0)

#endif /* _LINUXKPI_INCLUDE_ACPI_VIDEO_H_ */
