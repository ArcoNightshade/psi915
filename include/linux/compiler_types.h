/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Compiler attribute macros that kbuild force-includes into
 * every unit (-include linux/compiler_types.h). Imported DRM/i915 sources
 * use __must_check, __read_mostly, __iomem etc. without including
 * anything, so the P3 compile loop (and the future kernel build) passes
 * this header with -include, after linux/kconfig.h.
 */
#ifndef _LINUXKPI_LINUX_COMPILER_TYPES_H_
#define	_LINUXKPI_LINUX_COMPILER_TYPES_H_

#include <linux/compiler.h>
#include <linux/cache.h>

#endif /* _LINUXKPI_LINUX_COMPILER_TYPES_H_ */
