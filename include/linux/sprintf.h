/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Linux-semantics formatting (src/linux_printk.c): the names below are
 * macros over lkpi_* functions that understand the Linux %p extensions.
 * Environment headers declaring snprintf()/vsnprintf() are parsed before
 * this point (platform.h), so only Linux code sees the macros.
 */
#ifndef _LINUXKPI_INCLUDE_LINUX_SPRINTF_H_
#define	_LINUXKPI_INCLUDE_LINUX_SPRINTF_H_

#include <linuxkpi/platform.h>
#include <linux/types.h>
#include <linux/gfp.h>
#include <linux/compiler.h>

int lkpi_vsnprintf(char *buf, size_t size, const char *fmt, va_list ap)
    __printf(3, 0);
int lkpi_snprintf(char *buf, size_t size, const char *fmt, ...) __printf(3, 4);
int lkpi_vscnprintf(char *buf, size_t size, const char *fmt, va_list ap)
    __printf(3, 0);
int lkpi_scnprintf(char *buf, size_t size, const char *fmt, ...)
    __printf(3, 4);
int lkpi_sprintf(char *buf, const char *fmt, ...) __printf(2, 3);
char *kasprintf(gfp_t gfp, const char *fmt, ...) __printf(2, 3) __malloc;
char *kvasprintf(gfp_t gfp, const char *fmt, va_list ap) __printf(2, 0)
    __malloc;
const char *kvasprintf_const(gfp_t gfp, const char *fmt, va_list ap)
    __printf(2, 0);

#undef vsnprintf
#undef snprintf
#undef sprintf
#define	vsnprintf	lkpi_vsnprintf
#define	snprintf	lkpi_snprintf
#define	sprintf		lkpi_sprintf
#define	vscnprintf	lkpi_vscnprintf
#define	scnprintf	lkpi_scnprintf
#define	vsprintf(b, f, a)	lkpi_vsnprintf(b, (size_t)INT_MAX, f, a)

#endif /* _LINUXKPI_INCLUDE_LINUX_SPRINTF_H_ */
