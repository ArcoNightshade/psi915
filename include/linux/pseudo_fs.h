/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Pseudo file systems (init_pseudo); moved from drmglue
 * (docs/10, R1). Implemented in drmglue/src/pseudofs.c.
 */
#ifndef _LINUXKPI_LINUX_PSEUDO_FS_H_
#define	_LINUXKPI_LINUX_PSEUDO_FS_H_

#include <linux/fs_context.h>

struct super_operations;
struct xattr_handler;
struct dentry_operations;

struct pseudo_fs_context {
	const struct super_operations	*ops;
	const struct xattr_handler * const *xattr;
	const struct dentry_operations	*dops;
	unsigned long			magic;
};

struct pseudo_fs_context *init_pseudo(struct fs_context *fc,
    unsigned long magic);

#endif /* _LINUXKPI_LINUX_PSEUDO_FS_H_ */
