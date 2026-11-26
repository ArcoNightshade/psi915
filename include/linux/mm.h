/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation of the Linux page-level memory API from its
 * documented behaviour (src/linux_page.c):
 *
 *   alloc_pages(gfp, order)  physically contiguous, page-aligned memory
 *                            (lkpi_plat_contig_alloc: kmem for order 0,
 *                            ddi_dma_mem_alloc contiguous otherwise)
 *   page_address/kmap*       the permanent kernel address (no highmem)
 *   page_to_phys/pfn         CPU physical address (hat_getpfnum); for
 *                            device DMA use the DMA API, which returns
 *                            IOMMU-translated bus addresses
 *   virt_to_page/pfn_to_page only for memory from alloc_pages() and
 *                            __get_free_pages() (looked up in a table);
 *                            NULL (with a warning) for anything else
 *   vmap/vunmap              map pages into one contiguous kernel range
 *                            with a cache mode (illumos hat_devload)
 *   set_pages_xx, set_memory_xx change the kernel mapping's cache attribute
 *                            (<asm/set_memory.h>)
 *
 * VMA types (vm_area_struct, vm_fault, vm_operations_struct, VM_*) are in
 * <linux/mm_types.h>; the VMA helpers below (vmf_insert_pfn, remap_pfn_range,
 * unmap_mapping_range, ...) are implemented by the DRM glue (drmglue)
 * over illumos devmap. get_user_pages is not provided.
 */
#ifndef _LINUXKPI_LINUX_MM_H_
#define	_LINUXKPI_LINUX_MM_H_

#include <linuxkpi/platform.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/gfp.h>
#include <linux/mm_types.h>
#include <linux/pgtable.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/overflow.h>
#include <linux/err.h>
#include <linux/sizes.h>
#include <linux/fs.h>
#include <linux/shrinker.h>

#define	MAX_PAGE_ORDER		10
#define	MAX_ORDER		MAX_PAGE_ORDER
#define	NR_PAGE_ORDERS		(MAX_PAGE_ORDER + 1)

struct page *alloc_pages(gfp_t gfp, unsigned int order);
void __free_pages(struct page *page, unsigned int order);
unsigned long __get_free_pages(gfp_t gfp, unsigned int order);
void free_pages(unsigned long addr, unsigned int order);
struct page *lkpi_virt_to_page(const void *kva);
struct page *lkpi_pfn_to_page(unsigned long pfn);
void lkpi_page_release(struct page *head);
unsigned long lkpi_totalram_pages(void);

#define	alloc_page(gfp)			alloc_pages(gfp, 0)
#define	alloc_pages_node(n, gfp, o)	alloc_pages(gfp, o)
#define	__free_page(p)			__free_pages(p, 0)
#define	__get_free_page(gfp)		__get_free_pages(gfp, 0)
#define	get_zeroed_page(gfp)		__get_free_pages((gfp) | __GFP_ZERO, 0)
#define	free_page(addr)			free_pages(addr, 0)
#define	totalram_pages()		lkpi_totalram_pages()
#define	nth_page(p, n)			((p) + (n))

static inline struct page *
compound_head(struct page *p)
{
	return (p->lkpi_head != NULL ? p->lkpi_head : p);
}

static inline void *
page_address(const struct page *p)
{
	return (p->lkpi_kva);
}

#define	lowmem_page_address(p)		page_address(p)
#define	virt_to_page(kva)		lkpi_virt_to_page(kva)
#define	virt_to_head_page(kva)		compound_head(lkpi_virt_to_page(kva))
#define	pfn_to_page(pfn)		lkpi_pfn_to_page(pfn)
#define	page_to_pfn(p)			((unsigned long)((p)->lkpi_phys >> PAGE_SHIFT))
#define	page_to_phys(p)			((p)->lkpi_phys)
#define	pfn_valid(pfn)			(lkpi_pfn_to_page(pfn) != NULL)
#define	PageHighMem(p)			(false)
#define	PageReserved(p)			(false)
#define	PageSlab(p)			(false)
#define	PageCompound(p)			((p)->lkpi_head != NULL || (p)->lkpi_order > 0)
#define	SetPageReserved(p)		do { (void)(p); } while (0)
#define	ClearPageReserved(p)		do { (void)(p); } while (0)
#define	SetPageDirty(p)			do { (void)(p); } while (0)
#define	set_page_dirty(p)		((void)(p), 0)
#define	set_page_dirty_lock(p)		((void)(p), 0)
#define	mark_page_accessed(p)		do { (void)(p); } while (0)
#define	page_private(p)			((p)->private)
#define	set_page_private(p, v)		((p)->private = (v))
#define	compound_order(p)		(compound_head(p)->lkpi_order)
#define	page_size(p)			(PAGE_SIZE << compound_order(p))
#define	page_to_nid(p)			(0)

static inline int
page_count(struct page *p)
{
	return (atomic_read(&compound_head(p)->_refcount));
}

#define	page_ref_count(p)		page_count(p)

static inline void
get_page(struct page *p)
{
	atomic_inc(&compound_head(p)->_refcount);
}

static inline void
put_page(struct page *p)
{
	struct page *h = compound_head(p);

	if (atomic_dec_and_test(&h->_refcount))
		lkpi_page_release(h);
}

static inline bool
get_page_unless_zero(struct page *p)
{
	return (atomic_inc_not_zero(&compound_head(p)->_refcount));
}

#define	folio_get(f)			get_page(&(f)->page)
#define	folio_put(f)			put_page(&(f)->page)
#define	folio_address(f)		page_address(&(f)->page)

void *vmap(struct page **pages, unsigned int count, unsigned long flags,
    pgprot_t prot);
void vunmap(const void *addr);
void *vmap_pfn(unsigned long *pfns, unsigned int count, pgprot_t prot);
#define	VM_MAP			0x00000004UL
#define	VM_IOREMAP		0x00000001UL
#define	VM_MAP_PUT_PAGES	0x00000200UL

static inline unsigned int
get_order(unsigned long size)
{
	unsigned int order = 0;

	size = (size - 1) >> PAGE_SHIFT;
	while (size != 0) {
		order++;
		size >>= 1;
	}
	return (order);
}

/*
 * PTE-installing helpers (src/vma.c). addr is a user address inside the
 * vma; only (addr - vm_start) matters. A page that is already present is
 * left alone (Linux insert_pfn semantics).
 */
vm_fault_t vmf_insert_pfn(struct vm_area_struct *vma, unsigned long addr,
    unsigned long pfn);
vm_fault_t vmf_insert_pfn_prot(struct vm_area_struct *vma, unsigned long addr,
    unsigned long pfn, pgprot_t prot);
/*
 * VMA iteration over a user address space (i915 userptr probing). The
 * KPI does not see process mappings (illumos as/seg), so no VMA is ever
 * found and userptr objects are refused (-EFAULT): not supported yet.
 */
struct vma_iterator {
	struct mm_struct	*mm;
	unsigned long		index;
};
#define	VMA_ITERATOR(name, mm_, addr)					\
	struct vma_iterator name = { .mm = (mm_), .index = (addr) }

static inline struct vm_area_struct *
vma_find(struct vma_iterator *vmi, unsigned long max)
{
	(void) vmi;
	(void) max;
	return (NULL);
}
#define	for_each_vma_range(vmi, vma, end)				\
	for ((vma) = vma_find(&(vmi), (end)); (vma) != NULL;		\
	    (vma) = vma_find(&(vmi), (end)))

int remap_pfn_range(struct vm_area_struct *vma, unsigned long addr,
    unsigned long pfn, unsigned long size, pgprot_t prot);
#define	io_remap_pfn_range(v, a, p, s, pr)	remap_pfn_range(v, a, p, s, pr)
int vm_insert_page(struct vm_area_struct *vma, unsigned long addr,
    struct page *page);
vm_fault_t vmf_insert_page(struct vm_area_struct *vma, unsigned long addr,
    struct page *page);
void zap_vma_ptes(struct vm_area_struct *vma, unsigned long addr,
    unsigned long size);
void unmap_mapping_range(struct address_space *mapping, loff_t holebegin,
    loff_t holelen, int even_cows);

/*
 * Batched insertion of n contiguous-in-address pfns from pfns[] (or, if
 * pfns is NULL, pfn_base + i). For i915's remap_io_sg/remap_io_mapping
 * replacement (docs/10, R7). Returns 0 or -EINVAL/-EFAULT.
 */
int lkpi_vma_insert_pfns(struct vm_area_struct *vma, unsigned long addr,
    const unsigned long *pfns, unsigned long pfn_base, unsigned long n,
    pgprot_t prot);

struct sysinfo {
	unsigned long	totalram;
	unsigned long	freeram;
	unsigned long	totalhigh;
	unsigned int	mem_unit;
};

static inline void
si_meminfo(struct sysinfo *si)
{
	si->totalram = lkpi_totalram_pages();
	si->freeram = si->totalram / 2;		/* not tracked: a guess */
	si->totalhigh = 0;
	si->mem_unit = PAGE_SIZE;
}
#define	cond_resched_lockless()		do { } while (0)

/* Imported code expects copy_{to,from}_user from the mm headers. */
#include <linux/uaccess.h>
#include <linux/mmap_lock.h>

/* Page cache state of single-page folios (shmem backend: docs/04). */
#define	PageWriteback(p)		((void)(p), false)
#define	SetPageReclaim(p)		do { (void)(p); } while (0)
#define	ClearPageReclaim(p)		do { (void)(p); } while (0)
#define	page_mapped(p)			((void)(p), false)
#define	want_init_on_free()		(false)
#define	want_init_on_alloc(f)		((void)(f), false)

/* Copy-on-write private mapping (VM_MAYWRITE without VM_SHARED). */
#define	is_cow_mapping(flags)						\
	(((flags) & (VM_SHARED | VM_MAYWRITE)) == VM_MAYWRITE)

/*
 * Kernel-initiated user mmap (legacy I915_GEM_MMAP): not supported; the
 * ioctl fails and userspace uses the mmap-offset path.
 */
#define	vm_mmap(f, a, l, p, fl, off)					\
	((void)(f), (void)(a), (void)(l), (unsigned long)-ENODEV)

#endif /* _LINUXKPI_LINUX_MM_H_ */
