/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Cache prefetch hints (compiler builtins).
 */
#ifndef _LINUXKPI_LINUX_PREFETCH_H_
#define	_LINUXKPI_LINUX_PREFETCH_H_

#define	prefetch(x)		__builtin_prefetch(x)
#define	prefetchw(x)		__builtin_prefetch((x), 1)

#endif /* _LINUXKPI_LINUX_PREFETCH_H_ */
