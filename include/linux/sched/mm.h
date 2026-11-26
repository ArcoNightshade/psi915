/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Allocation-context scopes: illumos kmem_alloc() never
 * recurses into filesystem or I/O reclaim on behalf of the caller, so the
 * save/restore pairs are no-ops that return a cookie of 0.
 */
#ifndef _LINUXKPI_LINUX_SCHED_MM_H_
#define	_LINUXKPI_LINUX_SCHED_MM_H_

#include <linux/sched.h>

#define	memalloc_noreclaim_save()	((unsigned int)0)
#define	memalloc_noreclaim_restore(f)	do { (void)(f); } while (0)
#define	memalloc_nofs_save()		((unsigned int)0)
#define	memalloc_nofs_restore(f)	do { (void)(f); } while (0)
#define	memalloc_noio_save()		((unsigned int)0)
#define	memalloc_noio_restore(f)	do { (void)(f); } while (0)
#define	fs_reclaim_acquire(gfp)		do { (void)(gfp); } while (0)
#define	fs_reclaim_release(gfp)		do { (void)(gfp); } while (0)
#define	might_alloc(gfp)		do { (void)(gfp); } while (0)
#define	mmgrab(mm)			do { (void)(mm); } while (0)
#define	mmdrop(mm)			do { (void)(mm); } while (0)

#endif /* _LINUXKPI_LINUX_SCHED_MM_H_ */
