/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation from the documented behaviour of the Linux
 * kernel API; macro set cross-checked against FreeBSD's LinuxKPI
 * (sys/compat/linuxkpi/common/include/linux/compiler.h, BSD-2-Clause).
 */
#ifndef _LINUXKPI_LINUX_COMPILER_H_
#define	_LINUXKPI_LINUX_COMPILER_H_

#include <linuxkpi/platform.h>

/*
 * illumos <sys/ccompile.h> already provides some of these (__packed,
 * __aligned, __maybe_unused, __used, __section); only fill the gaps.
 */
#ifndef __packed
#define	__packed		__attribute__((__packed__))
#endif
#ifndef __aligned
#define	__aligned(x)		__attribute__((__aligned__(x)))
#endif
#ifndef __maybe_unused
#define	__maybe_unused		__attribute__((__unused__))
#endif
#ifndef __used
#define	__used			__attribute__((__used__))
#endif
#ifndef __always_inline
#define	__always_inline		inline __attribute__((__always_inline__))
#endif
#ifndef noinline
#define	noinline		__attribute__((__noinline__))
#endif
#ifndef __must_check
#define	__must_check		__attribute__((__warn_unused_result__))
#endif
#ifndef __printf
#define	__printf(a, b)		__attribute__((__format__(__printf__, a, b)))
#endif
#ifndef __malloc
#define	__malloc		__attribute__((__malloc__))
#endif
#ifndef __pure
#define	__pure			__attribute__((__pure__))
#endif

/* sparse annotations: no-ops for gcc/clang. */
#define	__user
#define	__kernel
#define	__iomem
#define	__force
#define	__rcu
#define	__percpu
#define	__bitwise
#define	__must_hold(x)
#define	__acquires(x)
#define	__releases(x)

/* Section markers: modules have no init/exit sections on illumos. */
#define	__init
#define	__exit
#define	__initdata
#define	__initconst

#ifndef typeof
#define	typeof(x)		__typeof__(x)
#endif

#define	likely(x)		__builtin_expect(!!(x), 1)
#define	unlikely(x)		__builtin_expect(!!(x), 0)

#define	barrier()		__asm__ __volatile__("" : : : "memory")

/*
 * Integer constant expression test that yields an integer constant
 * expression itself: a null pointer constant only arises when x is an ICE,
 * which changes the type of the conditional (well-known C idiom).
 */
#define	__is_constexpr(x)						\
	(sizeof(int) == sizeof(*(8 ? ((void *)((long)(x) * 0l)) : (int *)8)))

#define	__same_type(a, b)	__builtin_types_compatible_p(typeof(a), typeof(b))

/*
 * READ_ONCE/WRITE_ONCE: a single, non-elided, non-torn (for naturally
 * aligned word-sized objects) access through a volatile lvalue.
 */
#define	READ_ONCE(x)		(*(const volatile typeof(x) *)&(x))
#define	WRITE_ONCE(x, v)	do {					\
	*(volatile typeof(x) *)&(x) = (v);				\
} while (0)

#define	sizeof_field(t, m)	sizeof(((t *)0)->m)

#define	___PASTE(a, b)		a##b
#define	__PASTE(a, b)		___PASTE(a, b)
#define	__UNIQUE_ID(prefix)	__PASTE(__PASTE(__UNIQUE_ID_, prefix), __COUNTER__)

#endif /* _LINUXKPI_LINUX_COMPILER_H_ */
