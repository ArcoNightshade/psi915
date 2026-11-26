/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Mounts: struct vfsmount is the handle of a KPI pseudo
 * file system (kern_mount, drmglue/src/pseudofs.c).
 */
#ifndef _LINUXKPI_LINUX_MOUNT_H_
#define	_LINUXKPI_LINUX_MOUNT_H_

#include <linux/fs.h>

struct vfsmount {
	struct super_block	*mnt_sb;
};

struct vfsmount *kern_mount(struct file_system_type *type);
void kern_unmount(struct vfsmount *mnt);

#endif /* _LINUXKPI_LINUX_MOUNT_H_ */
