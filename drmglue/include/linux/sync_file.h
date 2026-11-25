/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * Linux sync_file kernel API, clean-room (Documentation/driver-api/
 * sync_file.rst). Linux's drivers/dma-buf/sync_file.c is GPL-2.0-only;
 * the copy imported under drm/drivers/dma-buf/ must not be built
 * (docs/10, requested change R9); drmglue/src/sync_file.c replaces it.
 *
 * Needs the KPI's <linux/dma-fence.h> (requested change R10); the unit
 * tests use a minimal stand-in (drmglue/tests/fakes/linux/dma-fence.h).
 */
#ifndef _LINUXKPI_LINUX_SYNC_FILE_H_
#define	_LINUXKPI_LINUX_SYNC_FILE_H_

#include <linux/types.h>
#include <linux/dma-fence.h>
#include <drmglue/lkpi_fs.h>

struct sync_file {
	struct lkpi_file	*file;
	char			user_name[32];
	unsigned long		flags;
	struct dma_fence	*fence;
	struct dma_fence_cb	cb;
};

#define	LKPI_SYNC_FILE_POLL_ENABLED	0

struct sync_file *sync_file_create(struct dma_fence *fence);
struct dma_fence *sync_file_get_fence(int fd);
char *sync_file_get_name(struct sync_file *sf, char *buf, int len);

#endif /* _LINUXKPI_LINUX_SYNC_FILE_H_ */
