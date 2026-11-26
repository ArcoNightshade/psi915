/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Stack traces are not captured (0 entries).
 */
#ifndef _LINUXKPI_LINUX_STACKTRACE_H_
#define	_LINUXKPI_LINUX_STACKTRACE_H_

#include <linux/types.h>

static inline unsigned int
stack_trace_save(unsigned long *store, unsigned int size, unsigned int skip)
{
	(void) store; (void) size; (void) skip;
	return (0);
}

#define	stack_trace_print(e, n, s)	do { } while (0)
#define	stack_trace_snprint(b, sz, e, n, s) (0)

#endif /* _LINUXKPI_LINUX_STACKTRACE_H_ */
