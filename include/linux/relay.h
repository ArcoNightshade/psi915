/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. No relay channels (i915 GuC log relay is unused on Gen9).
 */
#ifndef _LINUXKPI_INCLUDE_LINUX_RELAY_H_
#define	_LINUXKPI_INCLUDE_LINUX_RELAY_H_

#include <linux/types.h>

struct rchan;
struct rchan_buf;
struct rchan_callbacks {
	int	(*subbuf_start)(struct rchan_buf *, void *, void *, size_t);
};
#define	relay_open(n, p, s, c, cb, pd)	((struct rchan *)NULL)
#define	relay_close(c)			do { (void)(c); } while (0)
#define	relay_flush(c)			do { (void)(c); } while (0)
#define	relay_reserve(c, l)		((void *)NULL)

#endif /* _LINUXKPI_INCLUDE_LINUX_RELAY_H_ */
