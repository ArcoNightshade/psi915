/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. eventfd is not supported: drm_syncobj eventfd waits fail with
 * -ENODEV (illumos has eventfd(3C) but no kernel consumer API yet).
 */
#ifndef _LINUXKPI_INCLUDE_LINUX_EVENTFD_H_
#define	_LINUXKPI_INCLUDE_LINUX_EVENTFD_H_

#include <linux/types.h>
#include <linux/err.h>

struct eventfd_ctx;
#define	eventfd_ctx_fdget(fd)		((struct eventfd_ctx *)ERR_PTR(-ENODEV))
#define	eventfd_ctx_put(c)		do { (void)(c); } while (0)
#define	eventfd_signal(c)		do { (void)(c); } while (0)

#endif /* _LINUXKPI_INCLUDE_LINUX_EVENTFD_H_ */
