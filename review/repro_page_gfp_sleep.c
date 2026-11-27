/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Reproducer for docs/15 R3-M1 and R3-M2.
 *
 * R3-M1: i915 shmem_sg_alloc_table() (gem/i915_gem_shmem.c:93-140) makes a
 * last attempt with mapping_gfp_mask(mapping) | __GFP_RETRY_MAYFAIL so that
 * an unsatisfiable GEM backing-store request fails with -ENOMEM instead of
 * invoking the OOM killer. alloc_pages() passes only
 * gfpflags_allow_blocking(gfp) to lkpi_plat_contig_alloc(), so that request
 * becomes kmem_cache_alloc(KM_SLEEP) on illumos: it never fails, it waits
 * for memory (and every GEM page is non-pageable kernel heap).
 *
 * R3-M2: <linux/pagemap.h> defines mapping_gfp_constraint(m, g) as g; Linux
 * defines it as mapping_gfp_mask(m) & g. i915's "no reclaim" first pass
 * (mapping_gfp_constraint(mapping, ~__GFP_RECLAIM)) therefore carries
 * __GFP_NOFAIL, and drm_gem_get_pages()'s
 *   BUG_ON(mapping_gfp_constraint(mapping, __GFP_DMA32) && pfn >= 4 GiB)
 * fires for every page above 4 GiB.
 *
 * Exits 1 while either bug is present.
 */
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/gfp.h>
#include <linux/pagemap.h>
#include <stdio.h>

static int last_cansleep = -1;

void *__real_lkpi_plat_contig_alloc(size_t, bool, bool, void **);

void *
__wrap_lkpi_plat_contig_alloc(size_t size, bool zero, bool cansleep,
    void **cookie)
{
	last_cansleep = cansleep;
	return (__real_lkpi_plat_contig_alloc(size, zero, cansleep, cookie));
}

int
main(void)
{
	struct address_space *m = NULL;
	gfp_t last_try = mapping_gfp_mask(m) | __GFP_RETRY_MAYFAIL |
	    __GFP_NOWARN;
	gfp_t noreclaim = mapping_gfp_constraint(m, ~__GFP_RECLAIM) |
	    __GFP_NORETRY | __GFP_NOWARN;
	struct page *p;
	int fail = 0;

	p = alloc_page(last_try | __GFP_ZERO);
	printf("alloc_page(GFP_KERNEL|__GFP_RETRY_MAYFAIL): cansleep=%d\n",
	    last_cansleep);
	if (last_cansleep == 1) {
		printf("FAIL R3-M1: __GFP_RETRY_MAYFAIL page allocation is "
		    "KM_SLEEP (never fails)\n");
		fail = 1;
	}
	if (p != NULL)
		__free_page(p);

	if (noreclaim & __GFP_NOFAIL) {
		printf("FAIL R3-M2: i915 no-reclaim shmem gfp carries "
		    "__GFP_NOFAIL (mapping_gfp_constraint returns its "
		    "argument)\n");
		fail = 1;
	}
	if (mapping_gfp_constraint(m, __GFP_DMA32) != 0) {
		printf("FAIL R3-M2: mapping_gfp_constraint(m, __GFP_DMA32) is "
		    "non-zero for a GFP_KERNEL mapping (drm_gem_get_pages "
		    "BUG_ON for pages above 4 GiB)\n");
		fail = 1;
	}
	if (!fail)
		printf("ok\n");
	return (fail);
}
