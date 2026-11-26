/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * Pseudo-filesystem objects for upstream dma-buf.c; see
 * <drmglue/pseudofs.h>. Clean-room from the documented Linux API.
 */

#include <linux/kernel.h>
#include <linux/bug.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/stdarg.h>
#include <drmglue/lkpi_fs.h>
#include <drmglue/pseudofs.h>

struct pseudo_fs_context *
init_pseudo(struct fs_context *fc, unsigned long magic)
{
	struct pseudo_fs_context *ctx;

	ctx = kzalloc(sizeof (*ctx), GFP_KERNEL);
	if (ctx == NULL)
		return (NULL);
	ctx->magic = magic;
	fc->fs_private = ctx;
	return (ctx);
}

struct vfsmount *
kern_mount(struct file_system_type *type)
{
	struct pseudo_fs_context *ctx;
	struct super_block *sb;
	struct vfsmount *mnt;
	struct fs_context fc;
	int r;

	memset(&fc, 0, sizeof (fc));
	fc.fs_type = type;
	if (type->init_fs_context != NULL) {
		r = type->init_fs_context(&fc);
		if (r != 0) {
			kfree(fc.fs_private);
			return (ERR_PTR(r));
		}
	}
	ctx = fc.fs_private;
	sb = kzalloc(sizeof (*sb), GFP_KERNEL);
	mnt = kzalloc(sizeof (*mnt), GFP_KERNEL);
	if (sb == NULL || mnt == NULL) {
		kfree(sb);
		kfree(mnt);
		kfree(ctx);
		return (ERR_PTR(-ENOMEM));
	}
	sb->s_type = type;
	if (ctx != NULL) {
		sb->s_magic = ctx->magic;
		sb->s_d_op = ctx->dops;
		sb->s_op = ctx->ops;
	}
	atomic_long_set(&sb->s_lkpi_next_ino, 0);
	mnt->mnt_sb = sb;
	kfree(ctx);
	return (mnt);
}

void
kill_anon_super(struct super_block *sb)
{
}

void
kern_unmount(struct vfsmount *mnt)
{
	if (IS_ERR_OR_NULL(mnt))
		return;
	if (mnt->mnt_sb->s_type->kill_sb != NULL)
		mnt->mnt_sb->s_type->kill_sb(mnt->mnt_sb);
	kfree(mnt->mnt_sb);
	kfree(mnt);
}

struct inode *
alloc_anon_inode(struct super_block *sb)
{
	struct inode *ip;

	ip = lkpi_inode_alloc(0, 0600);	/* Linux: S_IRUSR | S_IWUSR, no type */
	if (ip == NULL)
		return (ERR_PTR(-ENOMEM));
	ip->i_sb = sb;
	if (sb != NULL)
		ip->i_ino = (unsigned long)atomic_long_inc_return(
		    &sb->s_lkpi_next_ino);
	return (ip);
}

void
iput(struct inode *ip)
{
	lkpi_inode_put(ip);
}

void
ihold(struct inode *ip)
{
	atomic_inc(&ip->i_count);
}

void
inode_set_bytes(struct inode *ip, loff_t bytes)
{
	ip->i_lkpi_bytes = bytes;
}

struct lkpi_file *
alloc_file_pseudo(struct inode *ip, struct vfsmount *mnt, const char *name,
    int flags, const struct file_operations *fops)
{
	struct dentry *d;
	struct lkpi_file *f;
	fmode_t mode;

	d = kzalloc(sizeof (*d), GFP_KERNEL);
	if (d == NULL)
		return (ERR_PTR(-ENOMEM));
	atomic_set(&d->d_count, 1);
	d->d_sb = mnt != NULL ? mnt->mnt_sb : NULL;
	d->d_op = d->d_sb != NULL ? d->d_sb->s_d_op : NULL;
	strscpy((char *)d->d_iname, name, sizeof (d->d_iname));
	d->d_name.name = d->d_iname;
	d->d_name.len = (unsigned int)strlen((char *)d->d_iname);

	switch (flags & O_ACCMODE) {
	case O_RDONLY:
		mode = FMODE_READ;
		break;
	case O_WRONLY:
		mode = FMODE_WRITE;
		break;
	default:
		mode = FMODE_READ | FMODE_WRITE;
		break;
	}
	f = lkpi_file_alloc(LKPI_FILE_ANON, fops, ip, (unsigned int)flags,
	    mode);
	if (f == NULL) {
		kfree(d);	/* inode still belongs to the caller */
		return (ERR_PTR(-ENOMEM));
	}
	d->d_inode = ip;	/* the caller's inode reference moves here */
	f->f_path.mnt = mnt;
	f->f_path.dentry = d;
	return (f);
}

struct dentry *
dget(struct dentry *d)
{
	if (d != NULL)
		atomic_inc(&d->d_count);
	return (d);
}

void
dput(struct dentry *d)
{
	if (d == NULL || !atomic_dec_and_test(&d->d_count))
		return;
	if (d->d_op != NULL && d->d_op->d_release != NULL)
		d->d_op->d_release(d);
	iput(d->d_inode);
	kfree(d);
}

char *
dynamic_dname(char *buffer, int buflen, const char *fmt, ...)
{
	char tmp[64];
	va_list ap;
	int sz;

	va_start(ap, fmt);
	sz = lkpi_plat_vsnprintf(tmp, sizeof (tmp), fmt, ap) + 1;
	va_end(ap);
	if (sz > (int)sizeof (tmp) || sz > buflen)
		return (ERR_PTR(-ENAMETOOLONG));
	buffer += buflen - sz;
	memcpy(buffer, tmp, (size_t)sz);
	return (buffer);
}
