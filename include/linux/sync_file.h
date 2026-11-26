/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. sync_file. Clean-room declaration of the Linux 6.14 interface (member names and
 * order as upstream, because the GPL implementation in gpl/drivers/dma-buf
 * and drivers use them directly); the out-of-line functions live in the
 * separate GPL module (docs/08 5, docs/10 R10). Inline helpers are this
 * project's own, written from the documented behaviour.
 */
#ifndef _LINUXKPI_INCLUDE_LINUX_SYNC_FILE_H_
#define	_LINUXKPI_INCLUDE_LINUX_SYNC_FILE_H_

#include <linux/types.h>
#include <linux/fs.h>
#include <linux/wait.h>
#include <linux/list.h>
#include <linux/dma-fence.h>

struct sync_file {
	struct lkpi_file	*file;
	char			user_name[32];
	struct list_head	sync_file_list;
	wait_queue_head_t	wq;
	unsigned long		flags;
	struct dma_fence	*fence;
	struct dma_fence_cb	cb;
};

#define	POLL_ENABLED	0

struct sync_file *sync_file_create(struct dma_fence *fence);
struct dma_fence *sync_file_get_fence(int fd);
char *sync_file_get_name(struct sync_file *sync_file, char *buf, int len);

#endif /* _LINUXKPI_INCLUDE_LINUX_SYNC_FILE_H_ */
