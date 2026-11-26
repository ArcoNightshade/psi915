/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. shmem files backing GEM objects. Declarations only: the illumos
 * implementation (anonymous memory objects without swap, docs/02 item 4)
 * is DRM glue work for P3 and not provided yet.
 */
#ifndef _LINUXKPI_INCLUDE_LINUX_SHMEM_FS_H_
#define	_LINUXKPI_INCLUDE_LINUX_SHMEM_FS_H_

#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/pagemap.h>

struct vfsmount;
struct lkpi_file *shmem_file_setup(const char *name, loff_t size,
    unsigned long flags);
struct lkpi_file *shmem_file_setup_with_mnt(struct vfsmount *mnt,
    const char *name, loff_t size, unsigned long flags);
struct page *shmem_read_mapping_page_gfp(struct address_space *mapping,
    pgoff_t index, gfp_t gfp);
struct folio *shmem_read_folio_gfp(struct address_space *mapping,
    pgoff_t index, gfp_t gfp);
void shmem_truncate_range(struct inode *inode, loff_t start, loff_t end);
#define	shmem_read_mapping_page(m, i)	shmem_read_mapping_page_gfp(m, i, GFP_KERNEL)
#define	shmem_read_folio(m, i)		shmem_read_folio_gfp(m, i, GFP_KERNEL)

#endif /* _LINUXKPI_INCLUDE_LINUX_SHMEM_FS_H_ */
