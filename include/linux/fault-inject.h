/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. No fault injection: should_fail() is always false.
 */
#ifndef _LINUXKPI_LINUX_FAULT_INJECT_H_
#define	_LINUXKPI_LINUX_FAULT_INJECT_H_

#include <linux/types.h>

struct fault_attr {
	int	dummy;
};
#define	DECLARE_FAULT_ATTR(name)	struct fault_attr name = { 0 }
#define	should_fail(attr, size)		((void)(attr), false)
#define	fault_create_debugfs_attr(n, p, a) ((void *)NULL)

#endif /* _LINUXKPI_LINUX_FAULT_INJECT_H_ */
