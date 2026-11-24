/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation from the documented behaviour of the Linux
 * kernel API: negative errno values encoded in the top MAX_ERRNO addresses
 * of the pointer space.
 */
#ifndef _LINUXKPI_LINUX_ERR_H_
#define	_LINUXKPI_LINUX_ERR_H_

#include <linuxkpi/platform.h>
#include <linux/compiler.h>
#include <linux/errno.h>

#define	MAX_ERRNO	4095

#define	IS_ERR_VALUE(x)							\
	unlikely((unsigned long)(void *)(x) >= (unsigned long)-MAX_ERRNO)

static inline void * __must_check
ERR_PTR(long error)
{
	return ((void *)error);
}

static inline long __must_check
PTR_ERR(const void *ptr)
{
	return ((long)ptr);
}

static inline bool __must_check
IS_ERR(const void *ptr)
{
	return (IS_ERR_VALUE((unsigned long)ptr));
}

static inline bool __must_check
IS_ERR_OR_NULL(const void *ptr)
{
	return (unlikely(ptr == NULL) || IS_ERR_VALUE((unsigned long)ptr));
}

static inline void * __must_check
ERR_CAST(const void *ptr)
{
	return ((void *)(uintptr_t)ptr);
}

static inline int __must_check
PTR_ERR_OR_ZERO(const void *ptr)
{
	return (IS_ERR(ptr) ? (int)PTR_ERR(ptr) : 0);
}

#endif /* _LINUXKPI_LINUX_ERR_H_ */
