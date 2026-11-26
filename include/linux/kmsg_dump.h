/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. No kmsg dumpers (panic output stays on the illumos console).
 */
#ifndef _LINUXKPI_INCLUDE_LINUX_KMSG_DUMP_H_
#define	_LINUXKPI_INCLUDE_LINUX_KMSG_DUMP_H_

#include <linux/types.h>

enum kmsg_dump_reason {
	KMSG_DUMP_UNDEF,
	KMSG_DUMP_PANIC,
	KMSG_DUMP_OOPS,
	KMSG_DUMP_EMERG,
	KMSG_DUMP_SHUTDOWN
};

struct kmsg_dumper {
	void	(*dump)(struct kmsg_dumper *dumper, int detail);
};

#define	kmsg_dump_register(d)	((void)(d), -ENODEV)
#define	kmsg_dump_unregister(d)	((void)(d), 0)

#endif /* _LINUXKPI_INCLUDE_LINUX_KMSG_DUMP_H_ */
