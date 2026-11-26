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
 *
 * It must not include <linuxkpi/platform.h> (or anything that does): it
 * is seen before the illumos headers of glue files, which must come
 * before the platform renames. Every definition here is identical to the
 * one in <linux/compiler.h> or <linux/cache.h>, so the later definition
 * is a benign redefinition.
 */
#ifndef _LINUXKPI_LINUX_COMPILER_TYPES_H_
#define	_LINUXKPI_LINUX_COMPILER_TYPES_H_

#ifndef __must_check
#define	__must_check		__attribute__((__warn_unused_result__))
#endif
#ifndef __deprecated
#define	__deprecated
#endif
#define	__diag_push()
#define	__diag_pop()
#define	__diag_ignore_all(option, comment)
#define	__diag_ignore(compiler, version, option, comment)
#define	__read_mostly
#define	__ro_after_init

#endif /* _LINUXKPI_LINUX_COMPILER_TYPES_H_ */
