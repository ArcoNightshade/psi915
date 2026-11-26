/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Reproducer for docs/12 finding M1 (dma_map_sg maps sg_virt()..+length).
 *
 * On illumos every alloc_page() is a separate contig_alloc() with its own
 * kernel VA, while the physical pages handed out by the page freelist are
 * very often consecutive. Linux (sg_alloc_table_from_pages(), i915
 * shmem_sg_alloc_table()) merges pages whose PFNs are consecutive into one
 * scatterlist entry. dma_map_sg_attrs() then binds the KERNEL VA range
 * [page_address(first page), +length), which on illumos is NOT where the
 * second page lives: the device gets the bus address of whatever kernel
 * memory follows the first page (or the bind walks an unmapped VA).
 *
 * The userland platform makes phys == kva, which hides the bug. This test
 * links the real KPI sources against the userland platform and only
 * replaces lkpi_plat_virt_to_phys() (ld --wrap) with an illumos-like model:
 * physical addresses are handed out consecutively, kernel VAs are not.
 *
 * Build/run: make -C review   (exit 1 = bug present)
 */
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/mm.h>
#include <linuxkpi/fakedev.h>
#include <stdio.h>

/* illumos model: kva page -> phys, consecutive in allocation order. */
static struct { uintptr_t kva; u64 phys; } map[64];
static int nmap;
static u64 next_phys = 0x40000000ULL;

u64 __wrap_lkpi_plat_virt_to_phys(const void *kva);
u64
__wrap_lkpi_plat_virt_to_phys(const void *kva)
{
	uintptr_t pg = (uintptr_t)kva & ~(uintptr_t)(PAGE_SIZE - 1);
	int i;

	for (i = 0; i < nmap; i++)
		if (map[i].kva == pg)
			return (map[i].phys | ((uintptr_t)kva & (PAGE_SIZE - 1)));
	map[nmap].kva = pg;
	map[nmap].phys = next_phys;
	next_phys += PAGE_SIZE;
	return (map[nmap++].phys | ((uintptr_t)kva & (PAGE_SIZE - 1)));
}

static void dev_release(struct device *d) { (void) d; }

int
main(void)
{
	struct device dev = { 0 };
	u64 mask = 0;
	struct page *pages[2];
	struct sg_table sgt;
	void *seen;
	int bad = 0;

	device_initialize(&dev);
	dev.release = dev_release;
	dev.dma_mask = &mask;
	(void) dma_set_mask_and_coherent(&dev, DMA_BIT_MASK(39));

	pages[0] = alloc_page(GFP_KERNEL);
	pages[1] = alloc_page(GFP_KERNEL);
	printf("p0 kva %p pfn %#lx, p1 kva %p pfn %#lx\n",
	    page_address(pages[0]), page_to_pfn(pages[0]),
	    page_address(pages[1]), page_to_pfn(pages[1]));
	if (page_to_pfn(pages[1]) != page_to_pfn(pages[0]) + 1) {
		printf("model setup failed\n");
		return (2);
	}
	if (sg_alloc_table_from_pages(&sgt, pages, 2, 0, 2 * PAGE_SIZE,
	    GFP_KERNEL) != 0)
		return (2);
	printf("sg entries: %u, first length %u\n", sgt.orig_nents,
	    sgt.sgl[0].length);
	if (dma_map_sgtable(&dev, &sgt, DMA_BIDIRECTIONAL, 0) != 0)
		return (2);
	/* Which kernel VA does the device reach at bus offset PAGE_SIZE? */
	if (sgt.orig_nents == 1) {
		seen = lkpi_fake_dma_kva(sg_dma_address(sgt.sgl) + PAGE_SIZE, 1);
		printf("device page 1 -> kva %p, expected %p\n", seen,
		    page_address(pages[1]));
		bad = seen != page_address(pages[1]);
	}
	printf("%s\n", bad ? "FAIL: second page of a merged sg entry is "
	    "mapped from the wrong kernel memory" : "ok");
	return (bad);
}
