/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Unaligned loads and stores (memcpy-based, so the compiler
 * emits plain moves on amd64) with little/big-endian conversions.
 */
#ifndef _LINUXKPI_LINUX_UNALIGNED_H_
#define	_LINUXKPI_LINUX_UNALIGNED_H_

#include <linux/types.h>
#include <linux/string.h>

#define	get_unaligned(p)	({					\
	__typeof__(*(p)) __lkpi_ua;					\
	__builtin_memcpy(&__lkpi_ua, (p), sizeof (__lkpi_ua));		\
	__lkpi_ua;							\
})
#define	put_unaligned(v, p)	do {					\
	__typeof__(*(p)) __lkpi_ua = (v);				\
	__builtin_memcpy((p), &__lkpi_ua, sizeof (__lkpi_ua));		\
} while (0)

#define	__LKPI_UA(bits)							\
static inline u##bits							\
get_unaligned_le##bits(const void *p)					\
{									\
	u##bits v;							\
	__builtin_memcpy(&v, p, sizeof (v));				\
	return (v);							\
}									\
static inline u##bits							\
get_unaligned_be##bits(const void *p)					\
{									\
	return (__builtin_bswap##bits(get_unaligned_le##bits(p)));	\
}									\
static inline void							\
put_unaligned_le##bits(u##bits v, void *p)				\
{									\
	__builtin_memcpy(p, &v, sizeof (v));				\
}									\
static inline void							\
put_unaligned_be##bits(u##bits v, void *p)				\
{									\
	put_unaligned_le##bits(__builtin_bswap##bits(v), p);		\
}
__LKPI_UA(16)
__LKPI_UA(32)
__LKPI_UA(64)
#undef	__LKPI_UA

static inline u32
get_unaligned_le24(const void *p)
{
	const u8 *b = p;

	return (b[0] | (u32)b[1] << 8 | (u32)b[2] << 16);
}

static inline u32
get_unaligned_be24(const void *p)
{
	const u8 *b = p;

	return (b[2] | (u32)b[1] << 8 | (u32)b[0] << 16);
}

#endif /* _LINUXKPI_LINUX_UNALIGNED_H_ */
