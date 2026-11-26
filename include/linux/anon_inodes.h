/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Anonymous-inode files; implemented by the DRM glue (drmglue,
 * src/anonfile.c) over illumos descriptors.
 */
#ifndef _LINUXKPI_INCLUDE_LINUX_ANON_INODES_H_
#define	_LINUXKPI_INCLUDE_LINUX_ANON_INODES_H_

#include <linux/fs.h>

struct lkpi_file *anon_inode_getfile(const char *name,
    const struct file_operations *fops, void *priv, int flags);

#endif /* _LINUXKPI_INCLUDE_LINUX_ANON_INODES_H_ */
