/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * Minimal Linux pseudo-filesystem objects for the upstream (GPL, gpl/)
 * dma-buf.c and the anonymous files of sync_file.c and drm_syncobj.c:
 * struct dentry, dentry_operations, super_block, vfsmount, fs_context,
 * pseudo_fs_context, file_system_type, kern_mount, init_pseudo,
 * alloc_anon_inode, alloc_file_pseudo, dput, iput, inode_set_bytes,
 * dynamic_dname. Clean-room from the documented API (Documentation/
 * filesystems/vfs.rst and the kernel-doc); member names follow v6.14,
 * layouts are the glue's own.
 *
 * Semantics kept from Linux, which dma-buf.c depends on:
 *   - alloc_file_pseudo() takes over the caller's inode reference (the
 *     dentry owns it); on failure the caller still owns it (iput);
 *   - the dentry gets the superblock's s_d_op (init_pseudo ctx->dops);
 *   - final fput: f_op->release, then dput of f_path.dentry; the last
 *     dput calls d_op->d_release (dma_buf_release frees the dma_buf
 *     there), then drops the inode.
 *
 * REQUESTED CHANGE (docs/10, R1): belongs in include/linux/fs.h,
 * dcache.h, mount.h, fs_context.h and pseudo_fs.h of the KPI.
 */
#ifndef _DRMGLUE_PSEUDOFS_H_
#define	_DRMGLUE_PSEUDOFS_H_

#include <linux/types.h>
#include <linux/atomic.h>
#include <drmglue/lkpi_fs.h>

struct qstr {
	const unsigned char	*name;
	unsigned int		len;
};

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

struct super_operations;
struct xattr_handler;

struct super_block {
	unsigned long		s_magic;
	const struct dentry_operations *s_d_op;
	const struct super_operations *s_op;
	struct file_system_type	*s_type;
	atomic_long_t		s_lkpi_next_ino;
};

struct vfsmount {
	struct super_block	*mnt_sb;
};

struct fs_context {
	struct file_system_type	*fs_type;
	void			*fs_private;
};

struct pseudo_fs_context {
	const struct super_operations	*ops;
	const struct xattr_handler * const *xattr;
	const struct dentry_operations	*dops;
	unsigned long			magic;
};

struct file_system_type {
	const char	*name;
	int		fs_flags;
	int		(*init_fs_context)(struct fs_context *);
	void		(*kill_sb)(struct super_block *);
	struct module	*owner;
};

struct pseudo_fs_context *init_pseudo(struct fs_context *fc,
    unsigned long magic);
struct vfsmount *kern_mount(struct file_system_type *type);
void kern_unmount(struct vfsmount *mnt);
void kill_anon_super(struct super_block *sb);

struct inode *alloc_anon_inode(struct super_block *sb);
void iput(struct inode *ip);
void ihold(struct inode *ip);
void inode_set_bytes(struct inode *ip, loff_t bytes);

struct lkpi_file *alloc_file_pseudo(struct inode *ip, struct vfsmount *mnt,
    const char *name, int flags, const struct file_operations *fops);
struct dentry *dget(struct dentry *d);
void dput(struct dentry *d);

/* Formats into the end of buffer, as Linux d_dname helpers do. */
char *dynamic_dname(char *buffer, int buflen, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));

#endif /* _DRMGLUE_PSEUDOFS_H_ */
