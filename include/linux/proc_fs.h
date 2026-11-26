/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. No procfs: entries are never created (NULL), removal is a
 * no-op.
 */
#ifndef _LINUXKPI_LINUX_PROC_FS_H_
#define	_LINUXKPI_LINUX_PROC_FS_H_

#include <linux/types.h>

struct proc_dir_entry;

#define	proc_create(n, m, p, ops)		((struct proc_dir_entry *)NULL)
#define	proc_create_data(n, m, p, ops, d)	((struct proc_dir_entry *)NULL)
#define	proc_create_single(n, m, p, show)	((struct proc_dir_entry *)NULL)
#define	proc_mkdir(n, p)			((struct proc_dir_entry *)NULL)
#define	proc_remove(e)				do { (void)(e); } while (0)
#define	remove_proc_entry(n, p)			do { } while (0)

#endif /* _LINUXKPI_LINUX_PROC_FS_H_ */
