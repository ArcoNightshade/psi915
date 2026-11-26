/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * Linux file-layer types the DRM glue needs: struct file, struct inode,
 * struct address_space, struct file_operations, poll_table. Clean-room,
 * from the documented Linux 6.x kernel API; only the members DRM core,
 * i915, dma-buf, sync_file and drm_syncobj use are present.
 *
 * REQUESTED CHANGE (docs/10, R1): these belong in the KPI as
 * include/linux/fs.h, linux/poll.h and linux/kdev_t.h. They live here only
 * because the KPI core files are being edited concurrently. When the KPI
 * grows those headers this file must shrink to including them, keeping the
 * f_lkpi_* members (or moving them behind one opaque pointer).
 *
 * Name clash: illumos <sys/file.h> defines struct file (file_t). As with
 * struct mutex (include/linuxkpi/platform.h), every later use of the
 * identifier "file" is renamed to lkpi_file, so illumos headers must be
 * included before this one; file_t keeps its illumos meaning. Done in
 * userland too, so tests see the same names. REQUESTED CHANGE (R2): move
 * the rename into <linuxkpi/platform.h> next to the mutex one.
 */
#ifndef _DRMGLUE_LKPI_FS_H_
#define	_DRMGLUE_LKPI_FS_H_

#include <linux/fs.h>

/*
 * Glue-internal file lifecycle (src/file.c).
 */
void lkpi_address_space_init(struct address_space *m, struct inode *host);
struct inode *lkpi_inode_alloc(uint32_t rdev, umode_t mode);
void lkpi_inode_put(struct inode *ip);
struct lkpi_file *lkpi_file_alloc(enum lkpi_file_kind kind,
    const struct file_operations *fops, struct inode *ip, unsigned int flags,
    fmode_t mode);
/* Free a file whose release never ran (open failed). */
void lkpi_file_free(struct lkpi_file *f);

#endif /* _DRMGLUE_LKPI_FS_H_ */
