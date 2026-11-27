/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * struct page allocator, vmap and cache attributes (see <linux/mm.h>,
 * <asm/set_memory.h>). Clean-room.
 *
 * Every alloc_pages() block is entered in two XArrays, by kernel virtual
 * page number and by physical page number, so virt_to_page() and
 * pfn_to_page() work for KPI-allocated memory. vmap() records each
 * mapping's page count for vunmap().
 */

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/xarray.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/bug.h>
#include <asm/set_memory.h>
#include <linuxkpi/plat_dev.h>

static DEFINE_XARRAY(lkpi_pages_by_va);
static DEFINE_XARRAY(lkpi_pages_by_pfn);
static DEFINE_XARRAY(lkpi_vmaps);	/* va page number -> npages value */

struct page *
alloc_pages(gfp_t gfp, unsigned int order)
{
	size_t n = (size_t)1 << order, i;
	size_t size = PAGE_SIZE << order;
	struct page *pages;
	void *kva, *cookie;

	if (order > MAX_PAGE_ORDER)
		return (NULL);
	pages = kcalloc(n, sizeof (*pages), gfp & ~__GFP_ZERO);
	if (pages == NULL)
		return (NULL);
	kva = lkpi_plat_contig_alloc(size, (gfp & __GFP_ZERO) != 0,
	    gfpflags_allow_blocking(gfp), &cookie);
	if (kva == NULL) {
		kfree(pages);
		return (NULL);
	}
	for (i = 0; i < n; i++) {
		struct page *p = &pages[i];

		INIT_LIST_HEAD(&p->lru);
		p->lkpi_kva = (char *)kva + i * PAGE_SIZE;
		p->lkpi_phys = lkpi_plat_virt_to_phys(p->lkpi_kva);
		p->lkpi_head = i == 0 ? NULL : pages;
		if (xa_err(xa_store(&lkpi_pages_by_va,
		    (unsigned long)p->lkpi_kva >> PAGE_SHIFT, p, gfp)) != 0 ||
		    xa_err(xa_store(&lkpi_pages_by_pfn,
		    (unsigned long)(p->lkpi_phys >> PAGE_SHIFT), p, gfp)) != 0) {
			size_t j;

			for (j = 0; j <= i; j++) {
				(void) xa_erase(&lkpi_pages_by_va,
				    (unsigned long)pages[j].lkpi_kva >> PAGE_SHIFT);
				(void) xa_erase(&lkpi_pages_by_pfn,
				    (unsigned long)(pages[j].lkpi_phys >>
				    PAGE_SHIFT));
			}
			lkpi_plat_contig_free(kva, size, cookie);
			kfree(pages);
			return (NULL);
		}
	}
	pages[0].lkpi_order = order;
	pages[0].lkpi_cookie = cookie;
	atomic_set(&pages[0]._refcount, 1);
	return (pages);
}

/* Last reference of a head page dropped (put_page) or __free_pages(). */
void
lkpi_page_release(struct page *head)
{
	unsigned int order = head->lkpi_order;
	size_t n = (size_t)1 << order, i;

	for (i = 0; i < n; i++) {
		(void) xa_erase(&lkpi_pages_by_va,
		    (unsigned long)head[i].lkpi_kva >> PAGE_SHIFT);
		(void) xa_erase(&lkpi_pages_by_pfn,
		    (unsigned long)(head[i].lkpi_phys >> PAGE_SHIFT));
	}
	lkpi_plat_contig_free(head->lkpi_kva, PAGE_SIZE << order,
	    head->lkpi_cookie);
	kfree(head);
}

void
__free_pages(struct page *page, unsigned int order)
{
	if (page == NULL)
		return;
	WARN_ON(page->lkpi_head != NULL || page->lkpi_order != order);
	put_page(page);
}

unsigned long
__get_free_pages(gfp_t gfp, unsigned int order)
{
	struct page *p = alloc_pages(gfp, order);

	return (p != NULL ? (unsigned long)page_address(p) : 0);
}

void
lkpi_free_pages(unsigned long addr, unsigned int order)
{
	if (addr != 0)
		__free_pages(lkpi_virt_to_page((void *)addr), order);
}

struct page *
lkpi_virt_to_page(const void *kva)
{
	struct page *p = xa_load(&lkpi_pages_by_va,
	    (unsigned long)kva >> PAGE_SHIFT);

	WARN_ONCE(p == NULL, "linuxkpi: virt_to_page(%p) of memory not from "
	    "alloc_pages()\n", kva);
	return (p);
}

struct page *
lkpi_pfn_to_page(unsigned long pfn)
{
	return (xa_load(&lkpi_pages_by_pfn, pfn));
}

unsigned long
lkpi_totalram_pages(void)
{
	return (lkpi_plat_physmem_pages());
}

static int
lkpi_cache_of(pgprot_t prot)
{
	switch (lkpi_pgprot_cache(prot)) {
	case LKPI_CACHE_WC:
		return (LKPI_CACHE_ATTR_WC);
	case LKPI_CACHE_UC:
		return (LKPI_CACHE_ATTR_UC);
	default:
		return (LKPI_CACHE_ATTR_WB);
	}
}

static void *
lkpi_vmap_phys(u64 *phys, unsigned int count, pgprot_t prot)
{
	void *va = lkpi_plat_vmap(phys, count, lkpi_cache_of(prot));

	if (va == NULL)
		return (NULL);
	if (xa_err(xa_store(&lkpi_vmaps, (unsigned long)va >> PAGE_SHIFT,
	    xa_mk_value(count), GFP_KERNEL)) != 0) {
		lkpi_plat_vunmap(va, count);
		return (NULL);
	}
	return (va);
}

void *
vmap(struct page **pages, unsigned int count, unsigned long flags,
    pgprot_t prot)
{
	u64 *phys;
	unsigned int i;
	void *va;

	(void) flags;
	if (count == 0)
		return (NULL);
	phys = kvmalloc_array(count, sizeof (*phys), GFP_KERNEL);
	if (phys == NULL)
		return (NULL);
	for (i = 0; i < count; i++)
		phys[i] = page_to_phys(pages[i]);
	va = lkpi_vmap_phys(phys, count, prot);
	kvfree(phys);
	return (va);
}

void *
vmap_pfn(unsigned long *pfns, unsigned int count, pgprot_t prot)
{
	u64 *phys;
	unsigned int i;
	void *va;

	phys = kvmalloc_array(count, sizeof (*phys), GFP_KERNEL);
	if (phys == NULL)
		return (NULL);
	for (i = 0; i < count; i++)
		phys[i] = (u64)pfns[i] << PAGE_SHIFT;
	va = lkpi_vmap_phys(phys, count, prot);
	kvfree(phys);
	return (va);
}

void
vunmap(const void *addr)
{
	void *e;

	if (addr == NULL)
		return;
	e = xa_erase(&lkpi_vmaps, (unsigned long)addr >> PAGE_SHIFT);
	if (e == NULL) {
		WARN_ONCE(1, "linuxkpi: vunmap of unknown %p\n", addr);
		return;
	}
	lkpi_plat_vunmap((void *)(uintptr_t)addr, xa_to_value(e));
}

static int
lkpi_set_pages(struct page **pages, int n, int cache)
{
	int i, r = 0;

	for (i = 0; i < n && r == 0; i++)
		r = lkpi_plat_set_cache(page_address(pages[i]), PAGE_SIZE,
		    cache);
	return (r);
}

int set_pages_array_wc(struct page **p, int n) { return (lkpi_set_pages(p, n, LKPI_CACHE_ATTR_WC)); }
int set_pages_array_uc(struct page **p, int n) { return (lkpi_set_pages(p, n, LKPI_CACHE_ATTR_UC)); }
int set_pages_array_wb(struct page **p, int n) { return (lkpi_set_pages(p, n, LKPI_CACHE_ATTR_WB)); }

int
set_pages_wb(struct page *page, int n)
{
	return (lkpi_plat_set_cache(page_address(page), PAGE_SIZE * (size_t)n,
	    LKPI_CACHE_ATTR_WB));
}

int
set_pages_uc(struct page *page, int n)
{
	return (lkpi_plat_set_cache(page_address(page), PAGE_SIZE * (size_t)n,
	    LKPI_CACHE_ATTR_UC));
}

int
set_memory_wc(unsigned long addr, int numpages)
{
	return (lkpi_plat_set_cache((void *)addr, PAGE_SIZE * (size_t)numpages,
	    LKPI_CACHE_ATTR_WC));
}

int
set_memory_uc(unsigned long addr, int numpages)
{
	return (lkpi_plat_set_cache((void *)addr, PAGE_SIZE * (size_t)numpages,
	    LKPI_CACHE_ATTR_UC));
}

int
set_memory_wb(unsigned long addr, int numpages)
{
	return (lkpi_plat_set_cache((void *)addr, PAGE_SIZE * (size_t)numpages,
	    LKPI_CACHE_ATTR_WB));
}
