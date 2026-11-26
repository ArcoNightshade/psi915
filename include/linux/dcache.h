/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room from the documented API (Documentation/filesystems/vfs.rst).
 * Directory entries of the KPI pseudo file systems only (anonymous
 * files of dma-buf, sync_file, DRM); moved from drmglue (docs/10, R1).
 * Member names follow v6.14, layouts are the KPI's own. Implemented in
 * drmglue/src/pseudofs.c.
 */
#ifndef _LINUXKPI_LINUX_DCACHE_H_
#define	_LINUXKPI_LINUX_DCACHE_H_

#include <linux/types.h>
#include <linux/atomic.h>
#include <linux/fs.h>

struct qstr {
	const unsigned char	*name;
	unsigned int		len;
};

#define	QSTR_INIT(n, l)		{ .name = (const unsigned char *)(n), .len = (l) }

struct dentry_operations {
	char	*(*d_dname)(struct dentry *, char *, int);
	void	(*d_release)(struct dentry *);
};

#define	LKPI_DNAME_INLINE_LEN	40

struct dentry {
	atomic_t		d_count;
	struct inode		*d_inode;
	void			*d_fsdata;
	const struct dentry_operations *d_op;
	struct super_block	*d_sb;
	struct qstr		d_name;
	unsigned char		d_iname[LKPI_DNAME_INLINE_LEN];
};

static inline struct inode *
d_inode(const struct dentry *d)
{
	return (d->d_inode);
}

struct dentry *dget(struct dentry *d);
void dput(struct dentry *d);

/* Formats into the end of buffer, as Linux d_dname helpers do. */
char *dynamic_dname(char *buffer, int buflen, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));

#endif /* _LINUXKPI_LINUX_DCACHE_H_ */
