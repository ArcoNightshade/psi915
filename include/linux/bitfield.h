/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation from the documented behaviour of the Linux
 * kernel API (linux/bitfield.h); cross-checked against FreeBSD's LinuxKPI
 * bitfield.h (BSD-2-Clause). Masks must be contiguous and non-zero; unlike
 * Linux, constant masks are not checked at compile time.
 */
#ifndef _LINUXKPI_LINUX_BITFIELD_H_
#define	_LINUXKPI_LINUX_BITFIELD_H_

#include <linuxkpi/platform.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/bits.h>
#include <linux/build_bug.h>

/* Shift of the lowest set bit of a mask. */
#define	__bf_shf(x)		(__builtin_ffsll(x) - 1)

#define	FIELD_MAX(mask)		((typeof(mask))((mask) >> __bf_shf(mask)))
#define	FIELD_FIT(mask, val)						\
	(!((((typeof(mask))(val)) << __bf_shf(mask)) & ~(mask)))
#define	FIELD_PREP(mask, val)						\
	((typeof(mask))(((typeof(mask))(val) << __bf_shf(mask)) & (mask)))
#define	FIELD_GET(mask, reg)						\
	((typeof(mask))(((reg) & (mask)) >> __bf_shf(mask)))
#define	FIELD_PREP_CONST(mask, val)	FIELD_PREP(mask, val)

#define	__LKPI_BF_FUNCS(type)					\
static inline type							\
type##_encode_bits(type v, type field)					\
{									\
	return ((type)((v << __bf_shf(field)) & field));		\
}									\
static inline type							\
type##_get_bits(type v, type field)					\
{									\
	return ((type)((v & field) >> __bf_shf(field)));		\
}									\
static inline type							\
type##_replace_bits(type old, type val, type field)			\
{									\
	return ((type)((old & ~field) | type##_encode_bits(val, field))); \
}									\
static inline void							\
type##p_replace_bits(type *p, type val, type field)			\
{									\
	*p = type##_replace_bits(*p, val, field);			\
}

__LKPI_BF_FUNCS(u8)
__LKPI_BF_FUNCS(u16)
__LKPI_BF_FUNCS(u32)
__LKPI_BF_FUNCS(u64)

/* Little-endian host: the le variants are the plain ones. */
#define	le16_encode_bits(v, f)	u16_encode_bits(v, f)
#define	le32_encode_bits(v, f)	u32_encode_bits(v, f)
#define	le64_encode_bits(v, f)	u64_encode_bits(v, f)
#define	le16_get_bits(v, f)	u16_get_bits(v, f)
#define	le32_get_bits(v, f)	u32_get_bits(v, f)
#define	le64_get_bits(v, f)	u64_get_bits(v, f)

#endif /* _LINUXKPI_LINUX_BITFIELD_H_ */
