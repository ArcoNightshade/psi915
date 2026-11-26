/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. No kmemleak (use ::findleaks in mdb).
 */
#ifndef _LINUXKPI_LINUX_KMEMLEAK_H_
#define	_LINUXKPI_LINUX_KMEMLEAK_H_

#define	kmemleak_alloc(p, s, c, g)	do { } while (0)
#define	kmemleak_free(p)		do { } while (0)
#define	kmemleak_not_leak(p)		do { } while (0)
#define	kmemleak_ignore(p)		do { } while (0)
#define	kmemleak_update_trace(p)	do { } while (0)

#endif /* _LINUXKPI_LINUX_KMEMLEAK_H_ */
