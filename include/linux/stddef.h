/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation from the documented behaviour of the Linux
 * kernel API.
 */
#ifndef _LINUXKPI_LINUX_STDDEF_H_
#define	_LINUXKPI_LINUX_STDDEF_H_

#include <linuxkpi/platform.h>

#ifndef offsetof
#define	offsetof(t, m)		__builtin_offsetof(t, m)
#endif

#define	offsetofend(t, m)	(offsetof(t, m) + sizeof(((t *)0)->m))

/* A flexible array member allowed in a union or as the only member. */
#define	DECLARE_FLEX_ARRAY(type, name)					\
	struct {							\
		struct { } __empty_ ## name;				\
		type name[];						\
	}

#endif /* _LINUXKPI_LINUX_STDDEF_H_ */
