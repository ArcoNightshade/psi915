/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation from the documented behaviour of the Linux
 * kernel API.
 */
#ifndef _LINUXKPI_LINUX_BUILD_BUG_H_
#define	_LINUXKPI_LINUX_BUILD_BUG_H_

#include <linux/compiler.h>

/*
 * Linux's static_assert accepts one or two arguments; C11 <assert.h> only
 * two. Replace any libc definition with the Linux form.
 */
#undef	static_assert
#define	static_assert(expr, ...)	_Static_assert(expr, #expr)

/*
 * Evaluates to 0 (of type int) if e is false; compile error if e is true.
 * Usable inside expressions, unlike _Static_assert.
 */
#define	BUILD_BUG_ON_ZERO(e)	((int)(sizeof(struct { int:(-!!(e)); })))

#define	BUILD_BUG_ON_MSG(e, msg)	_Static_assert(!(e), msg)
#define	BUILD_BUG_ON(e)		BUILD_BUG_ON_MSG(e, "BUILD_BUG_ON(" #e ")")
#define	BUILD_BUG_ON_NOT_POWER_OF_2(n)				\
	BUILD_BUG_ON((n) == 0 || (((n) & ((n) - 1)) != 0))
#define	BUILD_BUG()		BUILD_BUG_ON_MSG(1, "BUILD_BUG")

/* Nonzero-size compile error if a is not an array (pointers rejected). */
#define	__must_be_array(a)	BUILD_BUG_ON_ZERO(__same_type((a), &(a)[0]))

#endif /* _LINUXKPI_LINUX_BUILD_BUG_H_ */
