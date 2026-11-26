/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. typecheck(type, x) is 1, and warns (incompatible pointer
 * comparison) when x is not of the given type.
 */
#ifndef _LINUXKPI_LINUX_TYPECHECK_H_
#define	_LINUXKPI_LINUX_TYPECHECK_H_

#define	typecheck(type, x)	({					\
	type __lkpi_tc_a;						\
	__typeof__(x) __lkpi_tc_b;					\
	(void)(&__lkpi_tc_a == &__lkpi_tc_b);				\
	1;								\
})

#define	typecheck_fn(type, fn)	({					\
	type __lkpi_tc_f = (fn);					\
	(void)__lkpi_tc_f;						\
})

#define	typecheck_pointer(x)	({					\
	__typeof__(x) __lkpi_tc_p __attribute__((__unused__));		\
	(void)sizeof(*__lkpi_tc_p);					\
	1;								\
})

#endif /* _LINUXKPI_LINUX_TYPECHECK_H_ */
