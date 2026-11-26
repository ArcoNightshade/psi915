/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. debugfs does not exist on illumos: creation functions
 * return ERR_PTR(-ENODEV), which Linux callers already treat as "debugfs
 * unavailable" and pass back to the other debugfs calls, which ignore it.
 */
#ifndef _LINUXKPI_LINUX_DEBUGFS_H_
#define	_LINUXKPI_LINUX_DEBUGFS_H_

#include <linux/types.h>
#include <linux/err.h>
#include <linux/seq_file.h>
#include <linux/fs.h>

struct dentry;

struct debugfs_reg32 {
	char		*name;
	unsigned long	offset;
};

struct debugfs_regset32 {
	const struct debugfs_reg32	*regs;
	int				nregs;
	void				*base;
};

#define	DEFINE_DEBUGFS_ATTRIBUTE(f, g, s, fmt)	DEFINE_SIMPLE_ATTRIBUTE(f, g, s, fmt)
#define	DEFINE_DEBUGFS_ATTRIBUTE_SIGNED(f, g, s, fmt)	\
	DEFINE_SIMPLE_ATTRIBUTE(f, g, s, fmt)

#define	LKPI_DEBUGFS_NONE	((struct dentry *)ERR_PTR(-ENODEV))

#define	debugfs_create_dir(n, p)		LKPI_DEBUGFS_NONE
#define	debugfs_create_file(n, m, p, d, f)	LKPI_DEBUGFS_NONE
#define	debugfs_create_file_unsafe(n, m, p, d, f) LKPI_DEBUGFS_NONE
#define	debugfs_create_file_size(n, m, p, d, f, s) LKPI_DEBUGFS_NONE
#define	debugfs_create_symlink(n, p, t)		LKPI_DEBUGFS_NONE
#define	debugfs_lookup(n, p)			((struct dentry *)NULL)
#define	debugfs_create_bool(n, m, p, v)		do { (void)(v); } while (0)
#define	debugfs_create_u8(n, m, p, v)		do { (void)(v); } while (0)
#define	debugfs_create_u16(n, m, p, v)		do { (void)(v); } while (0)
#define	debugfs_create_u32(n, m, p, v)		do { (void)(v); } while (0)
#define	debugfs_create_u64(n, m, p, v)		do { (void)(v); } while (0)
#define	debugfs_create_x32(n, m, p, v)		do { (void)(v); } while (0)
#define	debugfs_create_x64(n, m, p, v)		do { (void)(v); } while (0)
#define	debugfs_create_ulong(n, m, p, v)	do { (void)(v); } while (0)
#define	debugfs_create_atomic_t(n, m, p, v)	do { (void)(v); } while (0)
#define	debugfs_create_regset32(n, m, p, r)	do { (void)(r); } while (0)
#define	debugfs_create_blob(n, m, p, b)		LKPI_DEBUGFS_NONE
#define	debugfs_remove(d)			do { (void)(d); } while (0)
#define	debugfs_remove_recursive(d)		do { (void)(d); } while (0)
#define	debugfs_lookup_and_remove(n, p)		do { } while (0)
#define	debugfs_initialized()			(false)

#endif /* _LINUXKPI_LINUX_DEBUGFS_H_ */
