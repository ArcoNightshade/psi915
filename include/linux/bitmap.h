/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation from the documented behaviour of the Linux
 * kernel bitmap API (basic operations); cross-checked against FreeBSD's
 * LinuxKPI bitmap.h (BSD-2-Clause). Bits beyond nbits in the last word are ignored by
 * the predicates and may be clobbered by the bulk operations, as in Linux.
 */
#ifndef _LINUXKPI_LINUX_BITMAP_H_
#define	_LINUXKPI_LINUX_BITMAP_H_

#include <linuxkpi/platform.h>
#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/slab.h>

static inline void
bitmap_zero(unsigned long *dst, unsigned int nbits)
{
	unsigned int i;

	for (i = 0; i < BITS_TO_LONGS(nbits); i++)
		dst[i] = 0;
}

static inline void
bitmap_fill(unsigned long *dst, unsigned int nbits)
{
	unsigned int i;

	for (i = 0; i < BITS_TO_LONGS(nbits); i++)
		dst[i] = ~0UL;
}

static inline void
bitmap_copy(unsigned long *dst, const unsigned long *src, unsigned int nbits)
{
	unsigned int i;

	for (i = 0; i < BITS_TO_LONGS(nbits); i++)
		dst[i] = src[i];
}

#define	__LKPI_BITMAP_OP2(name, expr)					\
static inline void							\
bitmap_##name(unsigned long *dst, const unsigned long *a,		\
    const unsigned long *b, unsigned int nbits)				\
{									\
	unsigned int i;							\
									\
	for (i = 0; i < BITS_TO_LONGS(nbits); i++)			\
		dst[i] = (expr);					\
}

__LKPI_BITMAP_OP2(or, a[i] | b[i])
__LKPI_BITMAP_OP2(xor, a[i] ^ b[i])

/* bitmap_and/andnot return whether the result is non-empty. */
static inline bool
bitmap_and(unsigned long *dst, const unsigned long *a, const unsigned long *b,
    unsigned int nbits)
{
	unsigned long any = 0;
	unsigned int i, n = BITS_TO_LONGS(nbits);

	for (i = 0; i < n; i++) {
		dst[i] = a[i] & b[i];
		any |= i + 1 == n ? dst[i] & BITMAP_LAST_WORD_MASK(nbits) :
		    dst[i];
	}
	return (any != 0);
}

static inline bool
bitmap_andnot(unsigned long *dst, const unsigned long *a,
    const unsigned long *b, unsigned int nbits)
{
	unsigned long any = 0;
	unsigned int i, n = BITS_TO_LONGS(nbits);

	for (i = 0; i < n; i++) {
		dst[i] = a[i] & ~b[i];
		any |= i + 1 == n ? dst[i] & BITMAP_LAST_WORD_MASK(nbits) :
		    dst[i];
	}
	return (any != 0);
}

static inline void
bitmap_complement(unsigned long *dst, const unsigned long *src,
    unsigned int nbits)
{
	unsigned int i;

	for (i = 0; i < BITS_TO_LONGS(nbits); i++)
		dst[i] = ~src[i];
}

static inline bool
bitmap_empty(const unsigned long *src, unsigned int nbits)
{
	return (find_first_bit(src, nbits) == nbits);
}

static inline bool
bitmap_full(const unsigned long *src, unsigned int nbits)
{
	return (find_first_zero_bit(src, nbits) == nbits);
}

static inline bool
bitmap_equal(const unsigned long *a, const unsigned long *b,
    unsigned int nbits)
{
	unsigned int i, n = nbits / BITS_PER_LONG;

	for (i = 0; i < n; i++)
		if (a[i] != b[i])
			return (false);
	if (nbits % BITS_PER_LONG != 0 &&
	    ((a[n] ^ b[n]) & BITMAP_LAST_WORD_MASK(nbits)) != 0)
		return (false);
	return (true);
}

static inline bool
bitmap_intersects(const unsigned long *a, const unsigned long *b,
    unsigned int nbits)
{
	return (find_next_and_bit(a, b, nbits, 0) < nbits);
}

/* Whether every bit set in a is also set in b. */
static inline bool
bitmap_subset(const unsigned long *a, const unsigned long *b,
    unsigned int nbits)
{
	unsigned int i, n = BITS_TO_LONGS(nbits);
	unsigned long w;

	for (i = 0; i < n; i++) {
		w = a[i] & ~b[i];
		if (i + 1 == n)
			w &= BITMAP_LAST_WORD_MASK(nbits);
		if (w != 0)
			return (false);
	}
	return (true);
}

static inline unsigned int
bitmap_weight(const unsigned long *src, unsigned int nbits)
{
	unsigned int i, n = nbits / BITS_PER_LONG, w = 0;

	for (i = 0; i < n; i++)
		w += hweight_long(src[i]);
	if (nbits % BITS_PER_LONG != 0)
		w += hweight_long(src[n] & BITMAP_LAST_WORD_MASK(nbits));
	return (w);
}

/* Set/clear len bits starting at start (non-atomic). */
static inline void
bitmap_set(unsigned long *map, unsigned int start, unsigned int len)
{
	unsigned int i;

	for (i = start; i < start + len; i++)
		__set_bit(i, map);
}

static inline void
bitmap_clear(unsigned long *map, unsigned int start, unsigned int len)
{
	unsigned int i;

	for (i = start; i < start + len; i++)
		__clear_bit(i, map);
}

/* Copy between bitmaps and u32 arrays (identical layout on amd64 LE). */
static inline void
bitmap_from_arr32(unsigned long *bitmap, const uint32_t *buf,
    unsigned int nbits)
{
	unsigned int i;

	bitmap_zero(bitmap, nbits);
	for (i = 0; i < nbits; i++)
		if (buf[i / 32] & (1U << (i % 32)))
			__set_bit(i, bitmap);
}

static inline void
bitmap_to_arr32(uint32_t *buf, const unsigned long *bitmap,
    unsigned int nbits)
{
	unsigned int i;

	for (i = 0; i < BITS_TO_U32(nbits); i++)
		buf[i] = 0;
	for (i = 0; i < nbits; i++)
		if (test_bit(i, bitmap))
			buf[i / 32] |= 1U << (i % 32);
}

static inline unsigned long *
bitmap_alloc(unsigned int nbits, gfp_t gfp)
{
	return (kmalloc_array(BITS_TO_LONGS(nbits), sizeof (unsigned long),
	    gfp));
}

static inline unsigned long *
bitmap_zalloc(unsigned int nbits, gfp_t gfp)
{
	return (bitmap_alloc(nbits, gfp | __GFP_ZERO));
}

static inline void
bitmap_free(const unsigned long *bitmap)
{
	kfree(bitmap);
}

#endif /* _LINUXKPI_LINUX_BITMAP_H_ */
