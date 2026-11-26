/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Cache-line alignment (64-byte lines on x86-64).
 */
#ifndef _LINUXKPI_INCLUDE_LINUX_CACHE_H_
#define	_LINUXKPI_INCLUDE_LINUX_CACHE_H_

#include <linux/compiler.h>

#define	L1_CACHE_SHIFT		6
#define	L1_CACHE_BYTES		(1 << L1_CACHE_SHIFT)
#define	SMP_CACHE_BYTES		L1_CACHE_BYTES
#define	INTERNODE_CACHE_SHIFT	L1_CACHE_SHIFT
#define	____cacheline_aligned	__attribute__((__aligned__(SMP_CACHE_BYTES)))
#define	____cacheline_aligned_in_smp	____cacheline_aligned
#define	__cacheline_aligned	____cacheline_aligned
#define	__cacheline_aligned_in_smp	____cacheline_aligned
#define	____cacheline_internodealigned_in_smp	____cacheline_aligned
#define	__read_mostly
#define	__ro_after_init
#define	cache_line_size()	L1_CACHE_BYTES

#endif /* _LINUXKPI_INCLUDE_LINUX_CACHE_H_ */
