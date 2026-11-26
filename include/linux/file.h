/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. File references (see <linux/fs.h>; fput() is in drmglue).
 */
#ifndef _LINUXKPI_LINUX_FILE_H_
#define	_LINUXKPI_LINUX_FILE_H_

#include <linux/fs.h>

/* Descriptor table operations; implemented by drmglue (src/anonfile.c). */
int get_unused_fd_flags(unsigned int flags);
void put_unused_fd(unsigned int fd);
void fd_install(unsigned int fd, struct lkpi_file *f);
struct lkpi_file *fget(unsigned int fd);

#endif /* _LINUXKPI_LINUX_FILE_H_ */
