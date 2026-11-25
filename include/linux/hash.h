/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Multiplicative (Fibonacci) hashing as documented for the
 * Linux hash_32()/hash_64()/hash_long()/hash_ptr() helpers: multiply by a
 * constant derived from the golden ratio and keep the top `bits` bits.
 * bits must be in 1..32 (hash_32) or 1..64 (hash_64).
 */
#ifndef _LINUXKPI_LINUX_HASH_H_
#define	_LINUXKPI_LINUX_HASH_H_

#include <linux/types.h>
#include <linux/compiler.h>

#define	GOLDEN_RATIO_32		0x61C88647U
#define	GOLDEN_RATIO_64		0x61C8864680B583EBULL

static inline u32
__hash_32(u32 val)
{
	return (val * GOLDEN_RATIO_32);
}

static inline u32
hash_32(u32 val, unsigned int bits)
{
	return (__hash_32(val) >> (32 - bits));
}

static inline u32
hash_64(u64 val, unsigned int bits)
{
	return ((u32)((val * GOLDEN_RATIO_64) >> (64 - bits)));
}

#define	hash_long(val, bits)						\
	(sizeof (long) == 8 ? hash_64((u64)(val), bits) :		\
	    hash_32((u32)(val), bits))

static inline u32
hash_ptr(const void *ptr, unsigned int bits)
{
	return (hash_long((unsigned long)ptr, bits));
}

static inline u32
hash32_ptr(const void *ptr)
{
	unsigned long v = (unsigned long)ptr;

	return ((u32)(v ^ (v >> 32)));
}

#endif /* _LINUXKPI_LINUX_HASH_H_ */
