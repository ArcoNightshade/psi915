/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Magic SysRq keys: there are none on illumos; registration
 * succeeds and the handler is never called.
 */
#ifndef _LINUXKPI_LINUX_SYSRQ_H_
#define	_LINUXKPI_LINUX_SYSRQ_H_

#include <linux/types.h>

struct sysrq_key_op {
	void		(*handler)(u8 key);
	const char	*help_msg;
	const char	*action_msg;
	int		enable_mask;
};

#define	register_sysrq_key(k, op)	((void)(k), (void)(op), 0)
#define	unregister_sysrq_key(k, op)	((void)(k), (void)(op), 0)

#endif /* _LINUXKPI_LINUX_SYSRQ_H_ */
