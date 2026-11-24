/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation from the documented behaviour of the Linux
 * kernel API; cross-checked against FreeBSD's LinuxKPI container_of.h
 * (BSD-2-Clause).
 */
#ifndef _LINUXKPI_LINUX_CONTAINER_OF_H_
#define	_LINUXKPI_LINUX_CONTAINER_OF_H_

#include <linux/compiler.h>
#include <linux/stddef.h>

/*
 * Given a pointer to member m of a struct of type t, return a pointer to the
 * enclosing struct. The assignment to a typed temporary makes the compiler
 * reject a ptr whose type does not match the member.
 */
#define	container_of(ptr, t, m) ({					\
	const typeof(((t *)0)->m) *__lkpi_mptr = (ptr);			\
	(t *)(void *)((char *)__lkpi_mptr - offsetof(t, m));		\
})

#define	typeof_member(t, m)	typeof(((t *)0)->m)

#endif /* _LINUXKPI_LINUX_CONTAINER_OF_H_ */
