/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation from the documented behaviour of the Linux
 * kernel API; function set cross-checked against FreeBSD's LinuxKPI
 * bitops.h (BSD-2-Clause).
 *
 * Bit numbers index an array of unsigned long, least significant bit of
 * word 0 first. set_bit/clear_bit/change_bit are atomic and unordered;
 * test_and_* are atomic and fully ordered; the __ forms are non-atomic.
 *
 * Population count is done with shifts and masks, not __builtin_popcount:
 * with -mno-sse (kernel flags) gcc lowers that builtin to a call into
 * libgcc (__popcountdi2), which the illumos kernel does not provide.
 */
#ifndef _LINUXKPI_LINUX_BITOPS_H_
#define	_LINUXKPI_LINUX_BITOPS_H_

#include <linuxkpi/platform.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/bits.h>
#include <asm/barrier.h>

#define	BITS_TO_LONGS(n)	(((n) + BITS_PER_LONG - 1) / BITS_PER_LONG)
#define	BITS_TO_U64(n)		(((n) + 63) / 64)
#define	BITS_TO_U32(n)		(((n) + 31) / 32)
#define	BITS_TO_BYTES(n)	(((n) + BITS_PER_BYTE - 1) / BITS_PER_BYTE)

/*
 * ffs/fls and friends. illumos <sys/systm.h> declares a kernel ffs() and
 * glibc <strings.h> a libc one, so the Linux names are macros over lkpi_
 * functions (an extern declaration after a static one keeps internal
 * linkage, so either include order works for the libc case).
 */

/* 1-based index of the least significant set bit; 0 if none. */
static inline int
lkpi_ffs(int x)
{
	return (__builtin_ffs(x));
}
#define	ffs(x)		lkpi_ffs(x)

/* 1-based index of the most significant set bit; 0 if none. */
static inline int
lkpi_fls(unsigned int x)
{
	return (x == 0 ? 0 : 32 - __builtin_clz(x));
}
#define	fls(x)		lkpi_fls(x)

static inline int
fls64(uint64_t x)
{
	return (x == 0 ? 0 : 64 - __builtin_clzll(x));
}

static inline unsigned int
fls_long(unsigned long x)
{
	return ((unsigned int)fls64(x));
}

/* 0-based index of the least/most significant set bit; x must be != 0. */
static inline unsigned long
__ffs(unsigned long x)
{
	return ((unsigned long)__builtin_ctzl(x));
}

static inline unsigned long
__fls(unsigned long x)
{
	return ((unsigned long)(BITS_PER_LONG - 1 - __builtin_clzl(x)));
}

static inline unsigned int
__ffs64(uint64_t x)
{
	return ((unsigned int)__builtin_ctzll(x));
}

/* 0-based index of the least significant clear bit; x must be != ~0. */
#define	ffz(x)		__ffs(~(unsigned long)(x))

static inline unsigned int
hweight32(uint32_t w)
{
	w = w - ((w >> 1) & 0x55555555U);
	w = (w & 0x33333333U) + ((w >> 2) & 0x33333333U);
	w = (w + (w >> 4)) & 0x0f0f0f0fU;
	return ((w * 0x01010101U) >> 24);
}

static inline unsigned int
hweight64(uint64_t w)
{
	w = w - ((w >> 1) & 0x5555555555555555ULL);
	w = (w & 0x3333333333333333ULL) + ((w >> 2) & 0x3333333333333333ULL);
	w = (w + (w >> 4)) & 0x0f0f0f0f0f0f0f0fULL;
	return ((unsigned int)((w * 0x0101010101010101ULL) >> 56));
}

#define	hweight8(w)	hweight32((uint8_t)(w))
#define	hweight16(w)	hweight32((uint16_t)(w))
#define	hweight_long(w)	hweight64((unsigned long)(w))

static inline uint8_t
rol8(uint8_t w, unsigned int s)
{
	return ((uint8_t)((w << (s & 7)) | (w >> ((-s) & 7))));
}

static inline uint8_t
ror8(uint8_t w, unsigned int s)
{
	return ((uint8_t)((w >> (s & 7)) | (w << ((-s) & 7))));
}

static inline uint16_t
rol16(uint16_t w, unsigned int s)
{
	return ((uint16_t)((w << (s & 15)) | (w >> ((-s) & 15))));
}

static inline uint16_t
ror16(uint16_t w, unsigned int s)
{
	return ((uint16_t)((w >> (s & 15)) | (w << ((-s) & 15))));
}

static inline uint32_t
rol32(uint32_t w, unsigned int s)
{
	return ((w << (s & 31)) | (w >> ((-s) & 31)));
}

static inline uint32_t
ror32(uint32_t w, unsigned int s)
{
	return ((w >> (s & 31)) | (w << ((-s) & 31)));
}

static inline uint64_t
rol64(uint64_t w, unsigned int s)
{
	return ((w << (s & 63)) | (w >> ((-s) & 63)));
}

static inline uint64_t
ror64(uint64_t w, unsigned int s)
{
	return ((w >> (s & 63)) | (w << ((-s) & 63)));
}

/* Sign-extend value whose sign bit is bit index. */
static inline int32_t
sign_extend32(uint32_t value, int index)
{
	uint8_t shift = 31 - index;

	return ((int32_t)(value << shift) >> shift);
}

static inline int64_t
sign_extend64(uint64_t value, int index)
{
	uint8_t shift = 63 - index;

	return ((int64_t)(value << shift) >> shift);
}

/* log2 of count rounded up; -1 for 0. */
static inline int
get_count_order(unsigned int count)
{
	if (count == 0)
		return (-1);
	return (lkpi_fls(count) - 1 + ((count & (count - 1)) != 0));
}

static inline int
get_count_order_long(unsigned long count)
{
	if (count == 0)
		return (-1);
	return ((int)fls_long(count) - 1 + ((count & (count - 1)) != 0));
}

/* Atomic bit operations. */
static inline void
set_bit(unsigned long nr, volatile unsigned long *addr)
{
	(void) __atomic_fetch_or(&addr[BIT_WORD(nr)], BIT_MASK(nr),
	    __ATOMIC_RELAXED);
}

static inline void
clear_bit(unsigned long nr, volatile unsigned long *addr)
{
	(void) __atomic_fetch_and(&addr[BIT_WORD(nr)], ~BIT_MASK(nr),
	    __ATOMIC_RELAXED);
}

static inline void
change_bit(unsigned long nr, volatile unsigned long *addr)
{
	(void) __atomic_fetch_xor(&addr[BIT_WORD(nr)], BIT_MASK(nr),
	    __ATOMIC_RELAXED);
}

static inline bool
test_and_set_bit(unsigned long nr, volatile unsigned long *addr)
{
	return ((__atomic_fetch_or(&addr[BIT_WORD(nr)], BIT_MASK(nr),
	    __ATOMIC_SEQ_CST) & BIT_MASK(nr)) != 0);
}

static inline bool
test_and_clear_bit(unsigned long nr, volatile unsigned long *addr)
{
	return ((__atomic_fetch_and(&addr[BIT_WORD(nr)], ~BIT_MASK(nr),
	    __ATOMIC_SEQ_CST) & BIT_MASK(nr)) != 0);
}

static inline bool
test_and_change_bit(unsigned long nr, volatile unsigned long *addr)
{
	return ((__atomic_fetch_xor(&addr[BIT_WORD(nr)], BIT_MASK(nr),
	    __ATOMIC_SEQ_CST) & BIT_MASK(nr)) != 0);
}

/* Bit locks: acquire on set, release on clear. */
static inline bool
test_and_set_bit_lock(unsigned long nr, volatile unsigned long *addr)
{
	return ((__atomic_fetch_or(&addr[BIT_WORD(nr)], BIT_MASK(nr),
	    __ATOMIC_ACQUIRE) & BIT_MASK(nr)) != 0);
}

static inline void
clear_bit_unlock(unsigned long nr, volatile unsigned long *addr)
{
	(void) __atomic_fetch_and(&addr[BIT_WORD(nr)], ~BIT_MASK(nr),
	    __ATOMIC_RELEASE);
}

static inline bool
test_bit(unsigned long nr, const volatile unsigned long *addr)
{
	return ((__atomic_load_n(&addr[BIT_WORD(nr)], __ATOMIC_RELAXED) &
	    BIT_MASK(nr)) != 0);
}

static inline bool
test_bit_acquire(unsigned long nr, const volatile unsigned long *addr)
{
	return ((__atomic_load_n(&addr[BIT_WORD(nr)], __ATOMIC_ACQUIRE) &
	    BIT_MASK(nr)) != 0);
}

/* Non-atomic bit operations. */
static inline void
__set_bit(unsigned long nr, volatile unsigned long *addr)
{
	addr[BIT_WORD(nr)] |= BIT_MASK(nr);
}

static inline void
__clear_bit(unsigned long nr, volatile unsigned long *addr)
{
	addr[BIT_WORD(nr)] &= ~BIT_MASK(nr);
}

static inline void
__change_bit(unsigned long nr, volatile unsigned long *addr)
{
	addr[BIT_WORD(nr)] ^= BIT_MASK(nr);
}

static inline bool
__test_and_set_bit(unsigned long nr, volatile unsigned long *addr)
{
	bool old = (addr[BIT_WORD(nr)] & BIT_MASK(nr)) != 0;

	addr[BIT_WORD(nr)] |= BIT_MASK(nr);
	return (old);
}

static inline bool
__test_and_clear_bit(unsigned long nr, volatile unsigned long *addr)
{
	bool old = (addr[BIT_WORD(nr)] & BIT_MASK(nr)) != 0;

	addr[BIT_WORD(nr)] &= ~BIT_MASK(nr);
	return (old);
}

static inline bool
__test_and_change_bit(unsigned long nr, volatile unsigned long *addr)
{
	bool old = (addr[BIT_WORD(nr)] & BIT_MASK(nr)) != 0;

	addr[BIT_WORD(nr)] ^= BIT_MASK(nr);
	return (old);
}

static inline void
__clear_bit_unlock(unsigned long nr, volatile unsigned long *addr)
{
	__atomic_store_n(&addr[BIT_WORD(nr)],
	    addr[BIT_WORD(nr)] & ~BIT_MASK(nr), __ATOMIC_RELEASE);
}

#define	assign_bit(nr, addr, value)					\
	((value) ? set_bit(nr, addr) : clear_bit(nr, addr))
#define	__assign_bit(nr, addr, value)					\
	((value) ? __set_bit(nr, addr) : __clear_bit(nr, addr))

/* Mask of the valid bits in the last word of an nbits-bit bitmap. */
#define	BITMAP_FIRST_WORD_MASK(start)	(~0UL << ((start) % BITS_PER_LONG))
#define	BITMAP_LAST_WORD_MASK(nbits)	(~0UL >> (-(nbits) % BITS_PER_LONG))

/*
 * Searches. All return size when nothing is found. invert flips every word
 * (0 to search for set bits, ~0UL for clear bits).
 */
static inline unsigned long
__lkpi_find_next_bit(const unsigned long *addr, unsigned long size,
    unsigned long start, unsigned long invert)
{
	unsigned long idx, w, bit;

	if (start >= size)
		return (size);
	idx = BIT_WORD(start);
	w = (addr[idx] ^ invert) & BITMAP_FIRST_WORD_MASK(start);
	while (w == 0) {
		idx++;
		if (idx * BITS_PER_LONG >= size)
			return (size);
		w = addr[idx] ^ invert;
	}
	bit = idx * BITS_PER_LONG + __ffs(w);
	return (bit < size ? bit : size);
}

static inline unsigned long
find_next_bit(const unsigned long *addr, unsigned long size,
    unsigned long offset)
{
	return (__lkpi_find_next_bit(addr, size, offset, 0UL));
}

static inline unsigned long
find_next_zero_bit(const unsigned long *addr, unsigned long size,
    unsigned long offset)
{
	return (__lkpi_find_next_bit(addr, size, offset, ~0UL));
}

static inline unsigned long
find_first_bit(const unsigned long *addr, unsigned long size)
{
	return (__lkpi_find_next_bit(addr, size, 0, 0UL));
}

static inline unsigned long
find_first_zero_bit(const unsigned long *addr, unsigned long size)
{
	return (__lkpi_find_next_bit(addr, size, 0, ~0UL));
}

static inline unsigned long
find_last_bit(const unsigned long *addr, unsigned long size)
{
	unsigned long idx, w;

	if (size == 0)
		return (size);
	idx = BIT_WORD(size - 1);
	w = addr[idx] & BITMAP_LAST_WORD_MASK(size);
	for (;;) {
		if (w != 0)
			return (idx * BITS_PER_LONG + __fls(w));
		if (idx == 0)
			return (size);
		w = addr[--idx];
	}
}

/* Next bit set in both a1 and a2. */
static inline unsigned long
find_next_and_bit(const unsigned long *a1, const unsigned long *a2,
    unsigned long size, unsigned long offset)
{
	unsigned long idx, w, bit;

	if (offset >= size)
		return (size);
	idx = BIT_WORD(offset);
	w = a1[idx] & a2[idx] & BITMAP_FIRST_WORD_MASK(offset);
	while (w == 0) {
		idx++;
		if (idx * BITS_PER_LONG >= size)
			return (size);
		w = a1[idx] & a2[idx];
	}
	bit = idx * BITS_PER_LONG + __ffs(w);
	return (bit < size ? bit : size);
}

#define	for_each_set_bit(bit, addr, size)				\
	for ((bit) = find_first_bit((addr), (size));			\
	    (bit) < (size);						\
	    (bit) = find_next_bit((addr), (size), (bit) + 1))

#define	for_each_set_bit_from(bit, addr, size)				\
	for ((bit) = find_next_bit((addr), (size), (bit));		\
	    (bit) < (size);						\
	    (bit) = find_next_bit((addr), (size), (bit) + 1))

#define	for_each_clear_bit(bit, addr, size)				\
	for ((bit) = find_first_zero_bit((addr), (size));		\
	    (bit) < (size);						\
	    (bit) = find_next_zero_bit((addr), (size), (bit) + 1))

#define	for_each_clear_bit_from(bit, addr, size)			\
	for ((bit) = find_next_zero_bit((addr), (size), (bit));		\
	    (bit) < (size);						\
	    (bit) = find_next_zero_bit((addr), (size), (bit) + 1))

#endif /* _LINUXKPI_LINUX_BITOPS_H_ */
