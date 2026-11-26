/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. File-system mount contexts (only what init_pseudo()
 * users need); moved from drmglue (docs/10, R1).
 */
#ifndef _LINUXKPI_LINUX_FS_CONTEXT_H_
#define	_LINUXKPI_LINUX_FS_CONTEXT_H_

#include <linux/fs.h>

struct fs_context {
	struct file_system_type	*fs_type;
	void			*fs_private;
};

#endif /* _LINUXKPI_LINUX_FS_CONTEXT_H_ */
