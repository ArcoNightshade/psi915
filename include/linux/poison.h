/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Poison values (debug patterns).
 */
#ifndef _LINUXKPI_INCLUDE_LINUX_POISON_H_
#define	_LINUXKPI_INCLUDE_LINUX_POISON_H_

#define	POISON_POINTER_DELTA	0
#define	LIST_POISON1		((void *)0x100)
#define	LIST_POISON2		((void *)0x122)
#define	POISON_INUSE		0x5a
#define	POISON_FREE		0x6b
#define	POISON_END		0xa5

#endif /* _LINUXKPI_INCLUDE_LINUX_POISON_H_ */
