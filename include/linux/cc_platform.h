/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Confidential computing: never active.
 */
#ifndef _LINUXKPI_LINUX_CC_PLATFORM_H_
#define	_LINUXKPI_LINUX_CC_PLATFORM_H_

#include <linux/types.h>

enum cc_attr {
	CC_ATTR_MEM_ENCRYPT,
	CC_ATTR_HOST_MEM_ENCRYPT,
	CC_ATTR_GUEST_MEM_ENCRYPT,
	CC_ATTR_GUEST_STATE_ENCRYPT
};
#define	cc_platform_has(attr)	((void)(attr), false)

#endif /* _LINUXKPI_LINUX_CC_PLATFORM_H_ */
