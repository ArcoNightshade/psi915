/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. sync_file user ABI (ioctl numbers and structures as defined by the
 * Linux UAPI; values are ABI facts, this header is written independently).
 */
#ifndef _LINUXKPI_INCLUDE_UAPI_LINUX_SYNC_FILE_H_
#define	_LINUXKPI_INCLUDE_UAPI_LINUX_SYNC_FILE_H_

#include <linux/types.h>
#include <asm/ioctl.h>

struct sync_merge_data {
	char	name[32];
	__s32	fd2;
	__s32	fence;
	__u32	flags;
	__u32	pad;
};

struct sync_fence_info {
	char	obj_name[32];
	char	driver_name[32];
	__s32	status;
	__u32	flags;
	__u64	timestamp_ns;
};

struct sync_file_info {
	char	name[32];
	__s32	status;
	__u32	flags;
	__u32	num_fences;
	__u32	pad;
	__u64	sync_fence_info;
};

struct sync_set_deadline {
	__u64	deadline_ns;
	__u64	pad;
};

#define	SYNC_IOC_MAGIC		'>'
#define	SYNC_IOC_MERGE		_IOWR(SYNC_IOC_MAGIC, 3, struct sync_merge_data)
#define	SYNC_IOC_FILE_INFO	_IOWR(SYNC_IOC_MAGIC, 4, struct sync_file_info)
#define	SYNC_IOC_SET_DEADLINE	_IOW(SYNC_IOC_MAGIC, 5, struct sync_set_deadline)

#endif /* _LINUXKPI_INCLUDE_UAPI_LINUX_SYNC_FILE_H_ */
