/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Per-output video options; implemented by the BSD
 * drm_bsd/drivers/video/cmdline.c. video_bsd_set_option() is the psi915
 * extension the illumos glue uses in place of the Linux "video=" kernel
 * command line (docs/11 R-K6b).
 */
#ifndef _LINUXKPI_VIDEO_CMDLINE_H_
#define	_LINUXKPI_VIDEO_CMDLINE_H_

#include <linux/types.h>

const char *video_get_options(const char *name);
bool __video_get_options(const char *name, const char **option, bool is_of);
int video_bsd_set_option(const char *name, const char *option);

#endif /* _LINUXKPI_VIDEO_CMDLINE_H_ */
