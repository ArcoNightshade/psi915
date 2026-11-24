/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Unit tests for <linux/slab.h>, <linux/vmalloc.h>, <linux/gfp.h>,
 * <linux/overflow.h>, <linux/string.h> and bitmap_alloc.
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/string.h>
#include <linux/overflow.h>
#include <linux/bitmap.h>

#include "harness.h"

static long live0;

static void
quiet(int level, const char *msg)
{
	(void) level;
	(void) msg;
}

TEST(kmalloc_basic)
{
	char *p, *q;
	int i;

	live0 = atomic_long_read(&lkpi_kmalloc_live);
	p = kmalloc(100, GFP_KERNEL);
	CHECK(p != NULL);
	CHECK(((uintptr_t)p & 15) == 0);
	CHECK_EQ(ksize(p), 100);
	CHECK_EQ(atomic_long_read(&lkpi_kmalloc_live), live0 + 1);
	memset(p, 0xa5, 100);

	q = kzalloc(64, GFP_KERNEL);
	CHECK(q != NULL);
	for (i = 0; i < 64; i++)
		CHECK_EQ(q[i], 0);
	kfree(q);
	kfree(p);
	kfree(NULL);
	CHECK_EQ(atomic_long_read(&lkpi_kmalloc_live), live0);

	/* kmalloc(0) gives the non-NULL, non-dereferenceable ZERO_SIZE_PTR. */
	p = kmalloc(0, GFP_KERNEL);
	CHECK(p == ZERO_SIZE_PTR);
	CHECK(ZERO_OR_NULL_PTR(p));
	CHECK_EQ(ksize(p), 0);
	kfree(p);
	CHECK_EQ(atomic_long_read(&lkpi_kmalloc_live), live0);
}

TEST(kmalloc_limits)
{
	lkpi_printk_hook = quiet;
	CHECK(kmalloc(KMALLOC_MAX_SIZE + 1, GFP_KERNEL) == NULL);
	CHECK(kmalloc_array(SIZE_MAX / 2, 4, GFP_KERNEL) == NULL);
	CHECK(kcalloc(1UL << 40, 1UL << 40, GFP_KERNEL) == NULL);
	CHECK(kvmalloc(SIZE_MAX - 4, GFP_KERNEL | __GFP_NOWARN) == NULL);
	lkpi_printk_hook = NULL;
	CHECK_EQ(atomic_long_read(&lkpi_kmalloc_live), live0);
}

TEST(krealloc)
{
	char *p, *n;
	int i;

	p = krealloc(NULL, 8, GFP_KERNEL);
	CHECK(p != NULL);
	for (i = 0; i < 8; i++)
		p[i] = (char)i;
	n = krealloc(p, 4, GFP_KERNEL);	/* shrink keeps the block */
	CHECK(n == p);
	n = krealloc(p, 32, GFP_KERNEL | __GFP_ZERO);
	CHECK(n != NULL);
	for (i = 0; i < 8; i++)
		CHECK_EQ(n[i], i);
	for (i = 8; i < 32; i++)
		CHECK_EQ(n[i], 0);
	CHECK_EQ(ksize(n), 32);
	p = krealloc_array(n, 16, 4, GFP_KERNEL);
	CHECK(p != NULL);
	CHECK_EQ(ksize(p), 64);
	CHECK(krealloc(p, 0, GFP_KERNEL) == ZERO_SIZE_PTR);
	p = kvrealloc(NULL, 10, GFP_KERNEL);
	p = kvrealloc(p, 1UL << 23, GFP_KERNEL);	/* > KMALLOC_MAX_SIZE */
	CHECK(p != NULL);
	kvfree(p);
	CHECK_EQ(atomic_long_read(&lkpi_kmalloc_live), live0);
}

TEST(kvmalloc_vmalloc)
{
	unsigned char *p;
	unsigned int *a;

	p = kvmalloc(KMALLOC_MAX_SIZE * 2, GFP_KERNEL);
	CHECK(p != NULL);
	p[KMALLOC_MAX_SIZE * 2 - 1] = 1;
	kvfree(p);
	p = kvzalloc(4096, GFP_KERNEL);
	CHECK(p != NULL && p[4095] == 0);
	kfree(p);		/* interchangeable with kvfree */
	p = vmalloc(1 << 20);
	CHECK(p != NULL);
	CHECK(!is_vmalloc_addr(p));
	vfree(p);
	p = vzalloc(100);
	CHECK(p != NULL && p[99] == 0);
	vfree(p);
	a = kvcalloc(1000, sizeof (*a), GFP_KERNEL);
	CHECK(a != NULL && a[999] == 0);
	kvfree(a);
	a = vcalloc(10, sizeof (*a));
	CHECK(a != NULL && a[9] == 0);
	vfree(a);
	p = kmalloc(16, GFP_KERNEL);
	memset(p, 0x5a, 16);
	kfree_sensitive(p);
	kvfree_sensitive(kmalloc(8, GFP_ATOMIC), 8);
	CHECK_EQ(atomic_long_read(&lkpi_kmalloc_live), live0);
}

TEST(gfp_mapping)
{
	CHECK_EQ(lkpi_gfp_to_plat(GFP_KERNEL), LKPI_PLAT_SLEEP);
	CHECK_EQ(lkpi_gfp_to_plat(GFP_ATOMIC), LKPI_PLAT_NOSLEEP);
	CHECK_EQ(lkpi_gfp_to_plat(GFP_NOWAIT), LKPI_PLAT_NOSLEEP);
	CHECK_EQ(lkpi_gfp_to_plat(GFP_KERNEL | __GFP_NORETRY),
	    LKPI_PLAT_NOSLEEP_LAZY);
	CHECK_EQ(lkpi_gfp_to_plat(GFP_KERNEL | __GFP_RETRY_MAYFAIL),
	    LKPI_PLAT_NOSLEEP_LAZY);
	CHECK_EQ(lkpi_gfp_to_plat(GFP_NOWAIT | __GFP_NOFAIL), LKPI_PLAT_SLEEP);
	CHECK(gfpflags_allow_blocking(GFP_NOIO));
	CHECK(!gfpflags_allow_blocking(GFP_ATOMIC));
}

TEST(dup_and_string)
{
	char buf[8];
	char *s;
	const char *cs;
	u32 w[4];
	u8 bytes[6] = { 0, 0, 0, 7, 0, 0 };
	int src[3] = { 1, 2, 3 };
	int *ip;

	s = kstrdup("hello", GFP_KERNEL);
	CHECK(s != NULL && strcmp(s, "hello") == 0);
	kfree(s);
	CHECK(kstrdup(NULL, GFP_KERNEL) == NULL);
	s = kstrndup("hello", 3, GFP_KERNEL);
	CHECK(s != NULL && strcmp(s, "hel") == 0);
	kfree(s);
	s = kmemdup_nul("abcdef", 2, GFP_KERNEL);
	CHECK(s != NULL && strcmp(s, "ab") == 0);
	kfree(s);
	s = kmemdup("xyz", 4, GFP_KERNEL);
	CHECK(s != NULL && strcmp(s, "xyz") == 0);
	kfree(s);
	ip = kmemdup_array(src, 3, sizeof (int), GFP_KERNEL);
	CHECK(ip != NULL && ip[2] == 3);
	kfree(ip);
	cs = kstrdup_const("const", GFP_KERNEL);
	CHECK(cs != NULL && strcmp(cs, "const") == 0);
	kfree_const(cs);

	CHECK_EQ(strscpy(buf, "abc", sizeof (buf)), 3);
	CHECK(strcmp(buf, "abc") == 0);
	CHECK_EQ(strscpy(buf, "abcdefghij", sizeof (buf)), -E2BIG);
	CHECK(strcmp(buf, "abcdefg") == 0);
	CHECK_EQ(strscpy(buf, "x", 0), -E2BIG);
	memset(buf, 'z', sizeof (buf));
	CHECK_EQ(strscpy_pad(buf, "ab", sizeof (buf)), 2);
	CHECK(buf[2] == 0 && buf[7] == 0);
	memset32(w, 0xdeadbeef, 4);
	CHECK(w[0] == 0xdeadbeef && w[3] == 0xdeadbeef);
	CHECK(memchr_inv(bytes, 0, sizeof (bytes)) == &bytes[3]);
	CHECK(memchr_inv(bytes, 0, 3) == NULL);
	CHECK_EQ(str_has_prefix("drm:foo", "drm:"), 4);
	CHECK_EQ(str_has_prefix("drm:foo", "i915"), 0);
	CHECK(strstarts("i915.modeset", "i915"));
	memzero_explicit(w, sizeof (w));
	CHECK_EQ(w[1], 0);
	CHECK_EQ(atomic_long_read(&lkpi_kmalloc_live), live0);
}

struct flex {
	int	n;
	u64	items[];
};

TEST(overflow)
{
	unsigned int u;
	int i;
	u8 b;
	size_t sz;
	struct flex *f = NULL;

	CHECK(!check_add_overflow(1U, 2U, &u) && u == 3);
	CHECK(check_add_overflow(UINT_MAX, 1U, &u) && u == 0);
	CHECK(check_sub_overflow(0U, 1U, &u));
	CHECK(check_mul_overflow(INT_MAX, 2, &i));
	CHECK(!check_mul_overflow(1000, 1000, &i) && i == 1000000);
	CHECK(check_add_overflow(200, 100, &b));
	CHECK(!check_shl_overflow(1U, 31, &u) && u == 0x80000000U);
	CHECK(check_shl_overflow(1U, 32, &u));
	CHECK(check_shl_overflow(3U, 31, &u));
	CHECK(check_shl_overflow(-1, 1, &i));
	CHECK(overflows_type(256, u8));
	CHECK(!overflows_type(255, u8));
	CHECK(overflows_type(-1, unsigned int));
	CHECK_EQ(type_max(u8), 255);
	CHECK_EQ(type_min(s8), -128);
	CHECK(type_max(s64) == S64_MAX);
	CHECK(is_signed_type(int) && !is_signed_type(unsigned int));
	CHECK(size_mul(SIZE_MAX / 2, 3) == SIZE_MAX);
	CHECK(size_add(SIZE_MAX, 1) == SIZE_MAX);
	CHECK(size_sub(1, 2) == SIZE_MAX);
	CHECK_EQ(size_sub(5, 2), 3);
	CHECK_EQ(array_size(10, 4), 40);
	CHECK_EQ(array3_size(2, 3, 4), 24);
	sz = struct_size(f, items, 4);
	CHECK_EQ(sz, sizeof (struct flex) + 4 * sizeof (u64));
	CHECK_EQ(flex_array_size(f, items, 3), 24);
	CHECK(struct_size(f, items, SIZE_MAX / 4) == SIZE_MAX);
	CHECK_EQ(struct_size_t(struct flex, items, 1),
	    sizeof (struct flex) + 8);
	f = kzalloc(struct_size(f, items, 2), GFP_KERNEL);
	CHECK(f != NULL);
	f->items[1] = 9;
	kfree(f);
}

struct widget {
	int	magic;
	int	allocs;
	char	pad[40];
};

static atomic_t ctor_calls;

static void
widget_ctor(void *p)
{
	struct widget *w = p;

	w->magic = 0x1234;
	w->allocs = 0;
	atomic_inc(&ctor_calls);
}

TEST(kmem_cache)
{
	struct kmem_cache *c;
	struct widget *w[8];
	int i;

	atomic_set(&ctor_calls, 0);
	c = kmem_cache_create("lkpi_test_widget", sizeof (struct widget), 0,
	    SLAB_HWCACHE_ALIGN, widget_ctor);
	CHECK(c != NULL);
	CHECK_EQ(kmem_cache_size(c), sizeof (struct widget));
	for (i = 0; i < 8; i++) {
		w[i] = kmem_cache_alloc(c, GFP_KERNEL);
		CHECK(w[i] != NULL);
		CHECK_EQ(w[i]->magic, 0x1234);	/* constructed */
		CHECK(((uintptr_t)w[i] & (L1_CACHE_BYTES - 1)) == 0);
	}
	CHECK(atomic_read(&ctor_calls) > 0);
	for (i = 0; i < 8; i++)
		kmem_cache_free(c, w[i]);
	kmem_cache_free(c, NULL);
	CHECK_EQ(kmem_cache_shrink(c), 0);
	kmem_cache_destroy(c);
	kmem_cache_destroy(NULL);
}

struct gadget {
	u64	a;
	u64	b;
};

TEST(kmem_cache_macro)
{
	struct kmem_cache *c = KMEM_CACHE(gadget, 0);
	struct gadget *g;
	unsigned int before = linuxkpi_warn_count;

	CHECK(c != NULL);
	g = kmem_cache_zalloc(c, GFP_KERNEL);
	CHECK(g != NULL && g->a == 0 && g->b == 0);
	/* Destroying a cache with live objects warns and keeps the cache. */
	lkpi_printk_hook = quiet;
	kmem_cache_destroy(c);
	lkpi_printk_hook = NULL;
	CHECK_EQ(linuxkpi_warn_count, before + 1);
	/* The cache is still usable and can be destroyed once empty. */
	kmem_cache_free(c, g);
	kmem_cache_destroy(c);
	CHECK_EQ(linuxkpi_warn_count, before + 1);
}

TEST(bitmap_alloc)
{
	unsigned long *bm = bitmap_zalloc(130, GFP_KERNEL);

	CHECK(bm != NULL);
	CHECK(bitmap_empty(bm, 130));
	set_bit(129, bm);
	CHECK_EQ(bitmap_weight(bm, 130), 1);
	bitmap_free(bm);
	CHECK_EQ(atomic_long_read(&lkpi_kmalloc_live), live0);
}

#define	NTHREADS	8

static void
alloc_hammer(void *arg)
{
	void *p[16];
	int i, j;

	(void) arg;
	for (i = 0; i < 2000; i++) {
		for (j = 0; j < 16; j++)
			p[j] = kmalloc(16 + (size_t)(i + j) % 512, GFP_KERNEL);
		for (j = 0; j < 16; j++)
			kfree(p[j]);
	}
}

TEST(concurrent)
{
	harness_run_threads(NTHREADS, alloc_hammer, NULL);
	CHECK_EQ(atomic_long_read(&lkpi_kmalloc_live), live0);
}

LKPI_SUITE(slab)
{
	RUN(kmalloc_basic);
	RUN(kmalloc_limits);
	RUN(krealloc);
	RUN(kvmalloc_vmalloc);
	RUN(gfp_mapping);
	RUN(dup_and_string);
	RUN(overflow);
	RUN(kmem_cache);
	RUN(kmem_cache_macro);
	RUN(bitmap_alloc);
	RUN(concurrent);
}
