/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * panic(fmt, ...): the illumos kernel's panic(9F) has the Linux signature
 * already and is used directly; userland builds get a stderr+abort()
 * version (src/linux_printk.c).
 */
#ifndef _LINUXKPI_LINUX_PANIC_H_
#define	_LINUXKPI_LINUX_PANIC_H_

#include <linuxkpi/platform.h>
#include <linux/compiler.h>

#ifdef LINUXKPI_USERLAND
void lkpi_panic(const char *fmt, ...) __printf(1, 2)
    __attribute__((__noreturn__));
#define	panic(...)	lkpi_panic(__VA_ARGS__)
#endif

#define	oops_in_progress	(0)
#define	panic_timeout		(0)

#endif /* _LINUXKPI_LINUX_PANIC_H_ */
