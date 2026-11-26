/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. nomodeset: never set on illumos (use driver.conf to disable).
 */
#ifndef _LINUXKPI_VIDEO_NOMODESET_H_
#define	_LINUXKPI_VIDEO_NOMODESET_H_

#include <linux/types.h>

static inline bool
video_firmware_drivers_only(void)
{
	return (false);
}

#endif /* _LINUXKPI_VIDEO_NOMODESET_H_ */
