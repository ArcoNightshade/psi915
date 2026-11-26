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

/*
 * Statement form, as Linux's compiletime_assert(): when the condition is
 * true after optimisation (constant folding and inlining), a call to a
 * function carrying the error attribute survives and the compiler stops
 * with msg. Conditions such as !__builtin_constant_p(arg) in an inline
 * function therefore work as in Linux. Without optimisation (or with
 * -fsyntax-only) nothing is checked; a surviving call in an unoptimised
 * object shows up as an unresolved symbol (check-kmod).
 */
#define	__lkpi_bbo_cat2(a, b)	a ## b
#define	__lkpi_bbo_cat(a, b)	__lkpi_bbo_cat2(a, b)
#define	__lkpi_build_bug(e, msg, fn)	do {				\
	extern void fn(void) __attribute__((__error__(msg)));		\
	if (e)								\
		fn();							\
} while (0)
#define	BUILD_BUG_ON_MSG(e, msg)					\
	__lkpi_build_bug(e, msg,					\
	    __lkpi_bbo_cat(__lkpi_build_bug_failed_, __COUNTER__))
#define	BUILD_BUG_ON(e)		BUILD_BUG_ON_MSG(e, "BUILD_BUG_ON(" #e ")")
#define	BUILD_BUG_ON_NOT_POWER_OF_2(n)				\
	BUILD_BUG_ON((n) == 0 || (((n) & ((n) - 1)) != 0))
#define	BUILD_BUG()		BUILD_BUG_ON_MSG(1, "BUILD_BUG")

/* Nonzero-size compile error if a is not an array (pointers rejected). */
#define	__must_be_array(a)	BUILD_BUG_ON_ZERO(__same_type((a), &(a)[0]))

#endif /* _LINUXKPI_LINUX_BUILD_BUG_H_ */
