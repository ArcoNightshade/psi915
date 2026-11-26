/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Linux in-kernel dev_t encoding (12-bit major, 20-bit minor).
 */
#ifndef _LINUXKPI_LINUX_KDEV_T_H_
#define	_LINUXKPI_LINUX_KDEV_T_H_

#include <linux/fs.h>

#define	MINORBITS	LKPI_MINORBITS
#define	MINORMASK	LKPI_MINORMASK
#define	MKDEV(ma, mi)	LKPI_MKDEV(ma, mi)
#define	MAJOR(d)	LKPI_MAJOR(d)
#define	MINOR(d)	LKPI_MINOR(d)

#endif /* _LINUXKPI_LINUX_KDEV_T_H_ */
