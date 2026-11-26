/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room shmem files (Linux shmem_file_setup): an internal file with
 * its own inode of the requested size whose mapping is a KPI shmem page
 * cache (src/linux_shmem.c). The file is never installed in a descriptor
 * table; DRM/i915 use it as GEM backing store through the page cache, and
 * GEM mmap goes through the DRM device, not through this file (its f_op
 * has no mmap, so call_mmap() on it fails with -ENODEV).
 *
 * Part of linuxkpi (build/Makefile.files DRMGLUE_OBJS). Not in drmglue's
 * userland test library (it needs the KPI page allocator); compile-checked
 * by `make check-kmod`, and the page cache is unit-tested in tests/.
 */

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/shmem_fs.h>
#include <drmglue/lkpi_fs.h>

static const struct file_operations lkpi_shmem_fops = {
	.llseek = noop_llseek,
};

struct lkpi_file *
shmem_file_setup(const char *name, loff_t size, unsigned long flags)
{
	struct inode *ip;
	struct lkpi_file *f;

	(void) name;
	(void) flags;
	if (size < 0)
		return (ERR_PTR(-EINVAL));
	ip = lkpi_inode_alloc(0, S_IFREG | 0600);
	if (ip == NULL)
		return (ERR_PTR(-ENOMEM));
	ip->i_size = size;
	lkpi_shmem_mapping_init(&ip->i_data);
	f = lkpi_file_alloc(LKPI_FILE_SHMEM, &lkpi_shmem_fops, ip, O_RDWR,
	    FMODE_READ | FMODE_WRITE);
	if (f == NULL) {
		lkpi_inode_put(ip);
		return (ERR_PTR(-ENOMEM));
	}
	f->f_lkpi_own_inode = ip;	/* freed (with its pages) by the last fput */
	return (f);
}

/* No separate shmem mounts (i915's huge-page gemfs): same as above. */
struct lkpi_file *
shmem_file_setup_with_mnt(struct vfsmount *mnt, const char *name, loff_t size,
    unsigned long flags)
{
	(void) mnt;
	return (shmem_file_setup(name, size, flags));
}
