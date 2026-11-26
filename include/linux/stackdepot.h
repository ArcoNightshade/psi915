/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Stack depot: no stack traces are recorded; handles are always 0.
 */
#ifndef _LINUXKPI_LINUX_STACKDEPOT_H_
#define	_LINUXKPI_LINUX_STACKDEPOT_H_

#include <linux/types.h>
#include <linux/gfp.h>

typedef u32 depot_stack_handle_t;
#define	STACK_DEPOT_FLAG_CAN_ALLOC	0x1

static inline depot_stack_handle_t
stack_depot_save(unsigned long *entries, unsigned int n, gfp_t gfp)
{
	(void) entries; (void) n; (void) gfp;
	return (0);
}

static inline unsigned int
stack_depot_fetch(depot_stack_handle_t h, unsigned long **entries)
{
	(void) h;
	*entries = NULL;
	return (0);
}

static inline int
stack_depot_snprint(depot_stack_handle_t h, char *buf, size_t size, int spaces)
{
	(void) h; (void) spaces;
	if (size > 0)
		buf[0] = '\0';
	return (0);
}

#define	stack_depot_print(h)		do { (void)(h); } while (0)
#define	stack_depot_init()		(0)
#define	stack_depot_request_early_init() do { } while (0)

#endif /* _LINUXKPI_LINUX_STACKDEPOT_H_ */
