/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Unit tests for <linux/{bits,bitops,bitmap,bitfield,log2,math,math64,
 * minmax,limits}.h>.
 */

#include <linux/kernel.h>
#include <linux/bits.h>
#include <linux/bitops.h>
#include <linux/bitmap.h>
#include <linux/bitfield.h>
#include <linux/log2.h>
#include <linux/math.h>
#include <linux/math64.h>
#include <linux/minmax.h>
#include <linux/limits.h>

#include "harness.h"

/* Constant folding: these must be integer constant expressions. */
static_assert(ilog2(4096) == 12);
static_assert(roundup_pow_of_two(33) == 64);
static_assert(rounddown_pow_of_two(33) == 32);
static_assert(order_base_2(5) == 3);
static_assert(GENMASK(7, 4) == 0xf0);
static_assert(__is_constexpr(3 + 4));
struct ilog2_sized { int a[ilog2(64)]; };

TEST(bits)
{
	CHECK_EQ(BIT(3), 8);
	CHECK(BIT_ULL(40) == 0x10000000000ULL);
	CHECK_EQ(BIT_WORD(65), 1);
	CHECK_EQ(BIT_MASK(65), 2);
	CHECK(GENMASK(63, 0) == ~0UL);
	CHECK_EQ(GENMASK(0, 0), 1);
	CHECK(GENMASK_ULL(39, 32) == 0xff00000000ULL);
	CHECK_EQ(GENMASK_U32(31, 28), 0xf0000000U);
	CHECK_EQ(BITS_PER_TYPE(u16), 16);
	CHECK_EQ(BITS_TO_LONGS(65), 2);
	CHECK_EQ(BITS_TO_LONGS(64), 1);
	CHECK_EQ(BITS_TO_U32(33), 2);
	CHECK_EQ(sizeof(struct ilog2_sized), 6 * sizeof(int));
}

TEST(ffs_fls)
{
	CHECK_EQ(ffs(0), 0);
	CHECK_EQ(ffs(1), 1);
	CHECK_EQ(ffs(0x80), 8);
	CHECK_EQ(ffs((int)0x80000000), 32);
	CHECK_EQ(fls(0), 0);
	CHECK_EQ(fls(1), 1);
	CHECK_EQ(fls(0x80000000U), 32);
	CHECK_EQ(fls64(0), 0);
	CHECK_EQ(fls64(1ULL << 63), 64);
	CHECK_EQ(fls_long(0x100), 9);
	CHECK_EQ(__ffs(0x100UL), 8);
	CHECK_EQ(__fls(0x100UL), 8);
	CHECK_EQ(__fls(~0UL), 63);
	CHECK_EQ(__ffs64(1ULL << 50), 50);
	CHECK_EQ(ffz(0x7UL), 3);
	CHECK_EQ(get_count_order(0), -1);
	CHECK_EQ(get_count_order(1), 0);
	CHECK_EQ(get_count_order(5), 3);
	CHECK_EQ(get_count_order(8), 3);
	CHECK_EQ(get_count_order_long(1UL << 40), 40);
}

TEST(hweight_rotate)
{
	CHECK_EQ(hweight8(0xff), 8);
	CHECK_EQ(hweight8(0x1ff), 8);	/* truncated to 8 bits */
	CHECK_EQ(hweight16(0xf0f0), 8);
	CHECK_EQ(hweight32(0xffffffffU), 32);
	CHECK_EQ(hweight32(0x80000001U), 2);
	CHECK_EQ(hweight64(~0ULL), 64);
	CHECK_EQ(hweight64(0x8000000000000001ULL), 2);
	CHECK_EQ(hweight_long(0x5555UL), 8);
	CHECK_EQ(rol32(0x80000001U, 1), 3);
	CHECK_EQ(ror32(3U, 1), 0x80000001U);
	CHECK_EQ(rol32(0x12345678U, 0), 0x12345678U);
	CHECK(rol64(1ULL << 63, 1) == 1);
	CHECK(ror64(1, 1) == 1ULL << 63);
	CHECK_EQ(rol8(0x81, 1), 0x03);
	CHECK_EQ(ror16(0x0001, 4), 0x1000);
	CHECK_EQ(sign_extend32(0x80, 7), -128);
	CHECK_EQ(sign_extend32(0x7f, 7), 127);
	CHECK_EQ(sign_extend64(0xfffULL, 11), -1);
}

TEST(atomic_bits)
{
	DECLARE_BITMAP(bm, 130);

	bitmap_zero(bm, 130);
	set_bit(0, bm);
	set_bit(64, bm);
	set_bit(129, bm);
	CHECK(test_bit(0, bm));
	CHECK(test_bit(64, bm));
	CHECK(test_bit(129, bm));
	CHECK(!test_bit(1, bm));
	CHECK_EQ(bm[1], 1);
	clear_bit(64, bm);
	CHECK(!test_bit(64, bm));
	change_bit(5, bm);
	CHECK(test_bit(5, bm));
	CHECK(test_and_set_bit(5, bm));
	CHECK(!test_and_set_bit(6, bm));
	CHECK(test_and_clear_bit(6, bm));
	CHECK(!test_and_clear_bit(6, bm));
	CHECK(!test_and_change_bit(7, bm));
	CHECK(test_bit(7, bm));
	CHECK(!test_and_set_bit_lock(100, bm));
	CHECK(test_bit_acquire(100, bm));
	clear_bit_unlock(100, bm);
	CHECK(!test_bit(100, bm));
	assign_bit(9, bm, true);
	CHECK(test_bit(9, bm));
	assign_bit(9, bm, false);
	CHECK(!test_bit(9, bm));
}

TEST(nonatomic_bits)
{
	unsigned long w[2] = { 0, 0 };

	__set_bit(3, w);
	__set_bit(70, w);
	CHECK_EQ(w[0], 8);
	CHECK_EQ(w[1], 64);
	CHECK(__test_and_set_bit(3, w));
	CHECK(!__test_and_clear_bit(4, w));
	CHECK(__test_and_clear_bit(3, w));
	CHECK(!__test_and_change_bit(1, w));
	__change_bit(1, w);
	__clear_bit(70, w);
	CHECK_EQ(w[0], 0);
	CHECK_EQ(w[1], 0);
	__assign_bit(2, w, 1);
	CHECK_EQ(w[0], 4);
	__set_bit(8, w);
	__clear_bit_unlock(8, w);
	CHECK_EQ(w[0], 4);
}

TEST(find_bits)
{
	DECLARE_BITMAP(bm, 200);
	DECLARE_BITMAP(other, 200);
	unsigned long bit, n;

	bitmap_zero(bm, 200);
	CHECK_EQ(find_first_bit(bm, 200), 200);
	CHECK_EQ(find_last_bit(bm, 200), 200);
	CHECK_EQ(find_first_zero_bit(bm, 200), 0);
	CHECK_EQ(find_first_bit(bm, 0), 0);
	CHECK_EQ(find_next_bit(bm, 200, 250), 200);

	__set_bit(3, bm);
	__set_bit(63, bm);
	__set_bit(64, bm);
	__set_bit(199, bm);
	CHECK_EQ(find_first_bit(bm, 200), 3);
	CHECK_EQ(find_next_bit(bm, 200, 4), 63);
	CHECK_EQ(find_next_bit(bm, 200, 64), 64);
	CHECK_EQ(find_next_bit(bm, 200, 65), 199);
	CHECK_EQ(find_next_bit(bm, 199, 65), 199);	/* beyond size */
	CHECK_EQ(find_last_bit(bm, 200), 199);
	CHECK_EQ(find_last_bit(bm, 199), 64);
	CHECK_EQ(find_last_bit(bm, 64), 63);

	n = 0;
	for_each_set_bit(bit, bm, 200)
		n += bit;
	CHECK_EQ(n, 3 + 63 + 64 + 199);

	bitmap_fill(bm, 200);
	__clear_bit(70, bm);
	__clear_bit(150, bm);
	CHECK_EQ(find_first_zero_bit(bm, 200), 70);
	CHECK_EQ(find_next_zero_bit(bm, 200, 71), 150);
	CHECK_EQ(find_next_zero_bit(bm, 200, 151), 200);
	n = 0;
	for_each_clear_bit(bit, bm, 200)
		n++;
	CHECK_EQ(n, 2);
	bit = 100;
	n = 0;
	for_each_set_bit_from(bit, bm, 105)
		n++;
	CHECK_EQ(n, 5);

	bitmap_zero(other, 200);
	__set_bit(70, other);
	__set_bit(80, other);
	__set_bit(190, other);
	CHECK_EQ(find_next_and_bit(bm, other, 200, 0), 80);
	CHECK_EQ(find_next_and_bit(bm, other, 200, 81), 190);
	CHECK_EQ(find_next_and_bit(bm, other, 190, 81), 190);
}

TEST(bitmap_ops)
{
	DECLARE_BITMAP(a, 70);
	DECLARE_BITMAP(b, 70);
	DECLARE_BITMAP(d, 70);
	u32 arr[3];

	bitmap_zero(a, 70);
	bitmap_zero(b, 70);
	CHECK(bitmap_empty(a, 70));
	CHECK(!bitmap_full(a, 70));
	bitmap_set(a, 60, 8);
	CHECK_EQ(bitmap_weight(a, 70), 8);
	CHECK_EQ(bitmap_weight(a, 64), 4);
	CHECK(test_bit(67, a));
	CHECK(!test_bit(68, a));
	bitmap_clear(a, 62, 2);
	CHECK_EQ(bitmap_weight(a, 70), 6);

	bitmap_set(b, 0, 61);
	CHECK(bitmap_intersects(a, b, 70));
	CHECK(bitmap_and(d, a, b, 70));
	CHECK_EQ(bitmap_weight(d, 70), 1);
	CHECK(bitmap_andnot(d, a, b, 70));
	CHECK_EQ(bitmap_weight(d, 70), 5);
	bitmap_or(d, a, b, 70);
	CHECK_EQ(bitmap_weight(d, 70), 66);
	bitmap_xor(d, a, b, 70);
	CHECK_EQ(bitmap_weight(d, 70), 65);
	CHECK(!bitmap_subset(a, b, 70));
	bitmap_copy(d, a, 70);
	CHECK(bitmap_equal(d, a, 70));
	CHECK(bitmap_subset(a, d, 70));
	__set_bit(69, d);
	CHECK(!bitmap_equal(d, a, 70));
	CHECK(bitmap_equal(d, a, 69));

	bitmap_fill(a, 70);
	CHECK(bitmap_full(a, 70));
	bitmap_complement(d, a, 70);
	CHECK(bitmap_empty(d, 70));

	bitmap_zero(a, 70);
	__set_bit(0, a);
	__set_bit(33, a);
	__set_bit(65, a);
	bitmap_to_arr32(arr, a, 70);
	CHECK_EQ(arr[0], 1);
	CHECK_EQ(arr[1], 2);
	CHECK_EQ(arr[2], 2);
	bitmap_from_arr32(b, arr, 70);
	CHECK(bitmap_equal(a, b, 70));
}

TEST(bitfield)
{
	u32 reg = 0;

	CHECK_EQ(FIELD_PREP(GENMASK(7, 4), 0xa), 0xa0);
	CHECK_EQ(FIELD_PREP(GENMASK(7, 4), 0x1a), 0xa0);	/* truncated */
	CHECK_EQ(FIELD_GET(GENMASK(7, 4), 0x1a5), 0xa);
	CHECK_EQ(FIELD_MAX(GENMASK(11, 8)), 0xf);
	CHECK(FIELD_FIT(GENMASK(11, 8), 0xf));
	CHECK(!FIELD_FIT(GENMASK(11, 8), 0x10));
	CHECK(FIELD_GET(GENMASK_ULL(47, 40), 0xab0000000000ULL) == 0xab);
	CHECK_EQ(u32_encode_bits(3, 0x30), 0x30);
	CHECK_EQ(u32_get_bits(0x1234, 0xff0), 0x23);
	CHECK_EQ(u32_replace_bits(0xffff, 0, 0xf0), 0xff0f);
	u32p_replace_bits(&reg, 5, 0x700);
	CHECK_EQ(reg, 0x500);
	CHECK_EQ(u8_encode_bits(1, 0x80), 0x80);
	CHECK(u64_get_bits(0xf00000000ULL, 0xf00000000ULL) == 0xf);
}

TEST(log2)
{
	unsigned long v = 1000;
	u32 v32 = 1U << 31;
	u64 v64 = 1ULL << 40;

	CHECK_EQ(ilog2(v), 9);
	CHECK_EQ(ilog2(v32), 31);
	CHECK_EQ(ilog2(v64), 40);
	CHECK_EQ(ilog2(1), 0);
	CHECK(is_power_of_2(1));
	CHECK(is_power_of_2(1UL << 63));
	CHECK(!is_power_of_2(0));
	CHECK(!is_power_of_2(6));
	CHECK_EQ(roundup_pow_of_two(v), 1024);
	CHECK_EQ(roundup_pow_of_two(1), 1);
	v = 1024;
	CHECK_EQ(roundup_pow_of_two(v), 1024);
	CHECK_EQ(rounddown_pow_of_two(v + 1), 1024);
	CHECK_EQ(order_base_2(1), 0);
	CHECK_EQ(order_base_2(v), 10);
	CHECK_EQ(order_base_2(v + 1), 11);
	CHECK_EQ(bits_per(255), 8);
	CHECK_EQ(bits_per(256), 9);
	CHECK_EQ(bits_per(0), 1);
}

TEST(math64)
{
	u64 n = 1000000007ULL;
	u32 rem32;
	s32 srem;
	u64 rem64;
	u32 r;

	CHECK(div_u64_rem(n, 10, &rem32) == 100000000ULL);
	CHECK_EQ(rem32, 7);
	CHECK_EQ(div_s64_rem(-17, 5, &srem), -3);
	CHECK_EQ(srem, -2);
	CHECK(div64_u64_rem(1ULL << 40, 3, &rem64) == 366503875925ULL);
	CHECK_EQ(rem64, 1);
	CHECK_EQ(div_u64(100, 7), 14);
	CHECK_EQ(div_s64(-100, 7), -14);
	CHECK(div64_u64(~0ULL, 1ULL << 32) == 0xffffffffULL);
	CHECK_EQ(div64_s64(-(1LL << 40), 1LL << 20), -(1LL << 20));
	r = do_div(n, 1000);
	CHECK_EQ(r, 7);
	CHECK(n == 1000000ULL);
	CHECK(mul_u32_u32(0xffffffffU, 0xffffffffU) == 0xfffffffe00000001ULL);
	CHECK(mul_u64_u32_shr(1ULL << 63, 4, 3) == 1ULL << 62);
	CHECK(mul_u64_u64_shr(1ULL << 63, 1ULL << 63, 64) == 1ULL << 62);
	/* 128-bit intermediate: (2^63 * 2^10) / 2^20 = 2^53. */
	CHECK(mul_u64_u64_div_u64(1ULL << 63, 1ULL << 10, 1ULL << 20) == 1ULL << 53);
	CHECK(mul_u64_u64_div_u64(1000000, 3, 7) == 428571);
	CHECK(mul_u64_u32_div(1ULL << 40, 1000, 1024) == 1000ULL << 30);
	/* Quotient overflow: low 64 bits of the true quotient. */
	CHECK(mul_u64_u64_div_u64(~0ULL, ~0ULL, 1) == 1);
	CHECK_EQ(DIV64_U64_ROUND_UP(10, 4), 3);
	CHECK_EQ(DIV64_U64_ROUND_CLOSEST(10, 4), 3);
	CHECK_EQ(DIV64_U64_ROUND_CLOSEST(9, 4), 2);
	CHECK_EQ(DIV_U64_ROUND_UP(9, 3), 3);
	CHECK_EQ(DIV_U64_ROUND_CLOSEST(14, 4), 4);
}

TEST(math)
{
	long l = -5;
	int i = -7;

	CHECK_EQ(DIV_ROUND_UP(7, 2), 4);
	CHECK_EQ(DIV_ROUND_UP_ULL(7ULL, 2), 4);
	CHECK_EQ(DIV_ROUND_DOWN_ULL(7ULL, 2), 3);
	CHECK_EQ(DIV_ROUND_CLOSEST(7, 2), 4);
	CHECK_EQ(DIV_ROUND_CLOSEST(5, 3), 2);
	CHECK_EQ(DIV_ROUND_CLOSEST(-7, 2), -4);
	CHECK_EQ(DIV_ROUND_CLOSEST(-5, 3), -2);
	CHECK_EQ(DIV_ROUND_CLOSEST(7u, 2u), 4);
	CHECK_EQ(DIV_ROUND_CLOSEST_ULL(10ULL, 4), 3);
	CHECK_EQ(roundup(7, 5), 10);
	CHECK_EQ(roundup(10, 5), 10);
	CHECK_EQ(rounddown(7, 5), 5);
	CHECK_EQ(mult_frac(10, 3, 4), 7);
	CHECK(mult_frac(0x7fffffffffffffffLL, 3, 4) == 0x5fffffffffffffffLL);
	CHECK_EQ(abs(i), 7);
	CHECK_EQ(abs(l), 5);
	CHECK(sizeof(abs(l)) == sizeof(long));
	CHECK_EQ(abs_diff(3u, 10u), 7);
	CHECK_EQ(abs_diff(10, 3), 7);
	CHECK_EQ(reciprocal_scale(0x80000000U, 10), 5);
	CHECK_EQ(int_sqrt(0), 0);
	CHECK_EQ(int_sqrt(1), 1);
	CHECK_EQ(int_sqrt(15), 3);
	CHECK_EQ(int_sqrt(16), 4);
	CHECK_EQ(int_sqrt(1000000), 1000);
	CHECK_EQ(int_sqrt(~0UL), 0xffffffffUL);
	CHECK_EQ(int_sqrt64(1ULL << 40), 1 << 20);
}

TEST(minmax_limits)
{
	unsigned int z = 0, five = 5, three = 3;
	int neg = -1;

	CHECK_EQ(min_not_zero(z, five), 5);
	CHECK_EQ(min_not_zero(five, z), 5);
	CHECK_EQ(min_not_zero(five, three), 3);
	CHECK_EQ(min_not_zero(z, z), 0);
	CHECK(umin(neg, 5) == 5);	/* -1 promoted to unsigned */
	CHECK(umax(neg, 5) == 0xffffffffULL);
	CHECK(in_range(5, 5, 1));
	CHECK(!in_range(6, 5, 1));
	CHECK(!in_range(4, 5, 10));
	CHECK(in_range(14U, 5U, 10U));
	CHECK_EQ(INT_MAX, 0x7fffffff);
	CHECK_EQ(INT_MIN, -INT_MAX - 1);
	CHECK(UINT_MAX == 0xffffffffU);
	CHECK(LONG_MAX == 0x7fffffffffffffffL);
	CHECK(ULONG_MAX == ~0UL);
	CHECK(SIZE_MAX == ~(size_t)0);
	CHECK_EQ(U16_MAX, 0xffff);
}

TEST(bitmap_regions)
{
	DECLARE_BITMAP(m, 64);
	DECLARE_BITMAP(d, 64);
	int r;

	bitmap_zero(m, 64);
	CHECK_EQ(bitmap_find_free_region(m, 64, 2), 0);
	CHECK_EQ(bitmap_find_free_region(m, 64, 3), 8);
	CHECK_EQ(bitmap_find_free_region(m, 64, 2), 4);
	CHECK_EQ(bitmap_weight(m, 64), 16);
	bitmap_release_region(m, 8, 3);
	CHECK_EQ(bitmap_weight(m, 64), 8);
	r = bitmap_find_free_region(m, 64, 6);
	CHECK_EQ(r, -ENOMEM);
	bitmap_zero(m, 64);
	__set_bit(5, m);
	__set_bit(63, m);
	bitmap_shift_right(d, m, 4, 64);
	CHECK(test_bit(1, d) && test_bit(59, d));
	CHECK_EQ(bitmap_weight(d, 64), 2);
}

LKPI_SUITE(bitops)
{
	RUN(bitmap_regions);
	RUN(bits);
	RUN(ffs_fls);
	RUN(hweight_rotate);
	RUN(atomic_bits);
	RUN(nonatomic_bits);
	RUN(find_bits);
	RUN(bitmap_ops);
	RUN(bitfield);
	RUN(log2);
	RUN(math64);
	RUN(math);
	RUN(minmax_limits);
}
