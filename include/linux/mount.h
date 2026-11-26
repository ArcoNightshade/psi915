/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Mounts: struct vfsmount is the DRM glue's pseudo-file-system
 * handle (drmglue/include/drmglue/pseudofs.h).
 */
#ifndef _LINUXKPI_INCLUDE_LINUX_MOUNT_H_
#define	_LINUXKPI_INCLUDE_LINUX_MOUNT_H_

#include <linux/fs.h>

struct vfsmount;
void kern_unmount(struct vfsmount *mnt);

#endif /* _LINUXKPI_INCLUDE_LINUX_MOUNT_H_ */
