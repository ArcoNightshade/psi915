/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation from the documented behaviour of the Linux
 * kernel API (linux/limits.h). C limits come from the compiler's
 * predefined macros, so no environment <limits.h> is needed.
 */
#ifndef _LINUXKPI_LINUX_LIMITS_H_
#define	_LINUXKPI_LINUX_LIMITS_H_

#include <linuxkpi/platform.h>
#include <linux/types.h>

#ifndef INT_MAX
#define	INT_MAX		__INT_MAX__
#endif
#ifndef INT_MIN
#define	INT_MIN		(-INT_MAX - 1)
#endif
#ifndef UINT_MAX
#define	UINT_MAX	(INT_MAX * 2U + 1U)
#endif
#ifndef LONG_MAX
#define	LONG_MAX	__LONG_MAX__
#endif
#ifndef LONG_MIN
#define	LONG_MIN	(-LONG_MAX - 1L)
#endif
#ifndef ULONG_MAX
#define	ULONG_MAX	(LONG_MAX * 2UL + 1UL)
#endif
#ifndef LLONG_MAX
#define	LLONG_MAX	__LONG_LONG_MAX__
#endif
#ifndef LLONG_MIN
#define	LLONG_MIN	(-LLONG_MAX - 1LL)
#endif
#ifndef ULLONG_MAX
#define	ULLONG_MAX	(LLONG_MAX * 2ULL + 1ULL)
#endif
#ifndef SIZE_MAX
#define	SIZE_MAX	(~(size_t)0)
#endif
#ifndef SSIZE_MAX
#define	SSIZE_MAX	((ssize_t)(SIZE_MAX >> 1))
#endif
#define	PHYS_ADDR_MAX	(~(phys_addr_t)0)

#define	U8_MAX		((u8)~0U)
#define	S8_MAX		((s8)(U8_MAX >> 1))
#define	S8_MIN		((s8)(-S8_MAX - 1))
#define	U16_MAX		((u16)~0U)
#define	S16_MAX		((s16)(U16_MAX >> 1))
#define	S16_MIN		((s16)(-S16_MAX - 1))
#define	U32_MAX		((u32)~0U)
#define	S32_MAX		((s32)(U32_MAX >> 1))
#define	S32_MIN		((s32)(-S32_MAX - 1))
#define	U64_MAX		((u64)~0ULL)
#define	S64_MAX		((s64)(U64_MAX >> 1))
#define	S64_MIN		((s64)(-S64_MAX - 1))

#endif /* _LINUXKPI_LINUX_LIMITS_H_ */
