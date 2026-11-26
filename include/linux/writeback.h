/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Writeback control: the parameters of a page write-out
 * (shmem write-out of purgeable GEM objects).
 */
#ifndef _LINUXKPI_LINUX_WRITEBACK_H_
#define	_LINUXKPI_LINUX_WRITEBACK_H_

#include <linux/types.h>

enum writeback_sync_modes {
	WB_SYNC_NONE,
	WB_SYNC_ALL
};

struct writeback_control {
	long			nr_to_write;
	long			pages_skipped;
	long long		range_start;
	long long		range_end;
	enum writeback_sync_modes sync_mode;
	unsigned int		for_kupdate:1;
	unsigned int		for_background:1;
	unsigned int		tagged_writepages:1;
	unsigned int		for_reclaim:1;
	unsigned int		range_cyclic:1;
	unsigned int		for_sync:1;
};

#endif /* _LINUXKPI_LINUX_WRITEBACK_H_ */
