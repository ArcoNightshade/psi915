/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. mmap lock of a user address space. The KPI does not manage
 * user address spaces (illumos as/seg do; drmglue handles device
 * mappings), so these are no-ops that always succeed.
 */
#ifndef _LINUXKPI_LINUX_MMAP_LOCK_H_
#define	_LINUXKPI_LINUX_MMAP_LOCK_H_

#include <linux/types.h>

struct mm_struct;

#define	mmap_read_lock(mm)		do { (void)(mm); } while (0)
#define	mmap_read_unlock(mm)		do { (void)(mm); } while (0)
#define	mmap_read_lock_killable(mm)	((void)(mm), 0)
#define	mmap_write_lock(mm)		do { (void)(mm); } while (0)
#define	mmap_write_unlock(mm)		do { (void)(mm); } while (0)
#define	mmap_write_lock_killable(mm)	((void)(mm), 0)
#define	mmap_assert_locked(mm)		do { (void)(mm); } while (0)
#define	mmap_assert_write_locked(mm)	do { (void)(mm); } while (0)

#endif /* _LINUXKPI_LINUX_MMAP_LOCK_H_ */
