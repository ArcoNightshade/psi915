/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * Linux anon_inodes.h / file.h API over illumos descriptors (src/anonfile.c).
 * REQUESTED CHANGE (docs/10, R1): belongs in include/linux/anon_inodes.h
 * and include/linux/file.h of the KPI.
 */
#ifndef _DRMGLUE_ANONFILE_H_
#define	_DRMGLUE_ANONFILE_H_

#include <drmglue/lkpi_fs.h>

struct lkpi_file *anon_inode_getfile(const char *name,
    const struct file_operations *fops, void *priv, int flags);
int get_unused_fd_flags(unsigned int flags);
void put_unused_fd(unsigned int fd);
void fd_install(unsigned int fd, struct lkpi_file *f);
struct lkpi_file *fget(unsigned int fd);

#endif /* _DRMGLUE_ANONFILE_H_ */
