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
#include <linux/cleanup.h>

/* Descriptor table operations; implemented by drmglue (src/anonfile.c). */
int get_unused_fd_flags(unsigned int flags);
void put_unused_fd(unsigned int fd);
void fd_install(unsigned int fd, struct lkpi_file *f);
struct lkpi_file *fget(unsigned int fd);

/* struct fd (fdget/fdput) and CLASS(fd, f)(fd) as in v6.14. */
struct fd {
	struct lkpi_file	*file;
};

static inline struct fd
fdget(unsigned int fd)
{
	struct fd f = { fget(fd) };

	return (f);
}

static inline void
fdput(struct fd f)
{
	if (f.file != NULL)
		fput(f.file);
}

#define	fd_file(f)		((f).file)
#define	fd_empty(f)		((f).file == NULL)

DEFINE_CLASS(fd, struct fd, fdput(_T), fdget(fd), int fd)

#endif /* _LINUXKPI_LINUX_FILE_H_ */
