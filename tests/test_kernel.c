/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Unit tests for <linux/types.h>, <linux/kernel.h> and <linux/build_bug.h>.
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/build_bug.h>

#include "harness.h"

/* Compile-time checks. */
static_assert(sizeof(u8) == 1);
static_assert(sizeof(u16) == 2, "u16");
static_assert(sizeof(u32) == 4);
static_assert(sizeof(u64) == 8);
static_assert(sizeof(s64) == 8);
static_assert(sizeof(dma_addr_t) == 8);
static_assert(sizeof(phys_addr_t) == 8);
static_assert(BITS_PER_LONG == 64);	/* amd64 only */

TEST(types)
{
	DECLARE_BITMAP(bm, 65);

	CHECK_EQ(sizeof(bm), 2 * sizeof(long));
	CHECK_EQ(U8_MAX, 255);
	CHECK_EQ(S8_MIN, -128);
	CHECK_EQ(U16_MAX, 65535);
	CHECK_EQ(S32_MAX, 2147483647);
	CHECK(U64_MAX == 0xffffffffffffffffULL);
	CHECK(S64_MIN < 0);
}

TEST(array_size)
{
	int a[7];
	char b[3][5];

	CHECK_EQ(ARRAY_SIZE(a), 7);
	CHECK_EQ(ARRAY_SIZE(b), 3);
	CHECK_EQ(BUILD_BUG_ON_ZERO(0), 0);
	BUILD_BUG_ON(sizeof(a) != 7 * sizeof(int));
	BUILD_BUG_ON_NOT_POWER_OF_2(64);
}

static int side_effects;
static int
bump(int v)
{
	side_effects++;
	return (v);
}

TEST(min_max)
{
	side_effects = 0;
	CHECK_EQ(min(bump(3), bump(5)), 3);
	CHECK_EQ(max(bump(3), bump(5)), 5);
	CHECK_EQ(side_effects, 4);	/* each argument evaluated once */

	CHECK_EQ(min3(4, 2, 9), 2);
	CHECK_EQ(max3(4, 2, 9), 9);
	CHECK_EQ(min_t(int, -1, 1u), -1);
	CHECK(max_t(unsigned int, -1, 1) == 0xffffffffu);
	CHECK_EQ(clamp(15, 0, 10), 10);
	CHECK_EQ(clamp(-5, 0, 10), 0);
	CHECK_EQ(clamp(5, 0, 10), 5);
	CHECK_EQ(clamp_t(int, 300, 0, 200), 200);
	CHECK_EQ(clamp_t(u8, 250, 10, 200), 200);
	CHECK_EQ(clamp_val((u8)5, 10, 200), 10);

	{
		int x = 1, y = 2;

		swap(x, y);
		CHECK(x == 2 && y == 1);
	}
}

TEST(rounding)
{
	CHECK_EQ(DIV_ROUND_UP(10, 4), 3);
	CHECK_EQ(DIV_ROUND_UP(8, 4), 2);
	CHECK_EQ(DIV_ROUND_UP_ULL(1ULL << 40, 3), ((1ULL << 40) + 2) / 3);
	CHECK_EQ(ALIGN(13, 8), 16);
	CHECK_EQ(ALIGN(16, 8), 16);
	CHECK_EQ(ALIGN_DOWN(13, 8), 8);
	CHECK(IS_ALIGNED(4096, 4096));
	CHECK(!IS_ALIGNED(4097, 4096));
	CHECK_EQ(round_up(4097UL, 4096UL), 8192);
	CHECK_EQ(round_down(4097UL, 4096UL), 4096);
	CHECK_EQ(rounddown(17, 5), 15);
	CHECK(PTR_ALIGN((char *)0x1001, 16) == (char *)0x1010);
	CHECK_EQ(upper_32_bits(0x123456789abcdef0ULL), 0x12345678);
	CHECK_EQ(lower_32_bits(0x123456789abcdef0ULL), 0x9abcdef0);
	CHECK_EQ(upper_32_bits(7u), 0);
}

TEST(read_write_once)
{
	int v = 0;

	WRITE_ONCE(v, 7);
	CHECK_EQ(READ_ONCE(v), 7);
	CHECK(likely(1));
	CHECK(!unlikely(0));
	CHECK_EQ(sizeof_field(struct list_head, next), sizeof(void *));
}

int
main(void)
{
	RUN(types);
	RUN(array_size);
	RUN(min_max);
	RUN(rounding);
	RUN(read_write_once);
	return (harness_done("test_kernel"));
}
