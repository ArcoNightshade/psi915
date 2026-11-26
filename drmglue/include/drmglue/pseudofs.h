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
 * The types and prototypes now live in the KPI (docs/10, R1):
 * include/linux/fs.h, dcache.h, mount.h, fs_context.h and pseudo_fs.h;
 * this header only gathers them for the glue.
 */
#ifndef _DRMGLUE_PSEUDOFS_H_
#define	_DRMGLUE_PSEUDOFS_H_

#include <linux/types.h>
#include <linux/atomic.h>
#include <drmglue/lkpi_fs.h>
#include <linux/fs.h>
#include <linux/dcache.h>
#include <linux/mount.h>
#include <linux/fs_context.h>
#include <linux/pseudo_fs.h>

#endif /* _DRMGLUE_PSEUDOFS_H_ */
