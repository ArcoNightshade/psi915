/*
 * Copyright © 2014 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 */

#include <linux/mm.h>
#include <linux/io-mapping.h>


#include "i915_drv.h"
#include "i915_mm.h"

struct remap_pfn {
	struct mm_struct *mm;
	unsigned long pfn;
	pgprot_t prot;

	struct sgt_iter sgt;
	resource_size_t iobase;
};

#define use_dma(io) ((io) != -1)

static inline unsigned long sgt_pfn(const struct remap_pfn *r)
{
	if (use_dma(r->iobase))
		return (r->sgt.dma + r->sgt.curr + r->iobase) >> PAGE_SHIFT;
	else
		return r->sgt.pfn + (r->sgt.curr >> PAGE_SHIFT);
}

#ifdef CONFIG_PSI915
/*
 * psi915 (docs/10, R7): illumos has no page tables to fill with apply_to_page_range().  The
 * drmglue VMA layer loads translations through lkpi_vma_insert_pfns() into the devmap mapping
 * behind the VMA, so remap_io_mapping() and remap_io_sg() become loops over it.
 */
int lkpi_vma_insert_pfns(struct vm_area_struct *vma, unsigned long addr,
			 const unsigned long *pfns, unsigned long pfn_base,
			 unsigned long n, pgprot_t prot);

#define PSI915_EXPECTED_FLAGS (VM_PFNMAP | VM_DONTEXPAND | VM_DONTDUMP)
#define PSI915_REMAP_BATCH 64

int remap_io_mapping(struct vm_area_struct *vma,
		     unsigned long addr, unsigned long pfn, unsigned long size,
		     struct io_mapping *iomap)
{
	GEM_BUG_ON((vma->vm_flags & PSI915_EXPECTED_FLAGS) != PSI915_EXPECTED_FLAGS);

	/* The io_mapping's cache mode (write-combining) applies, as upstream. */
	return lkpi_vma_insert_pfns(vma, addr, NULL, pfn, size >> PAGE_SHIFT,
				    iomap->prot);
}

int remap_io_sg(struct vm_area_struct *vma,
		unsigned long addr, unsigned long size,
		struct scatterlist *sgl, unsigned long offset,
		resource_size_t iobase)
{
	struct remap_pfn r = {
		.prot = vma->vm_page_prot,
		.sgt = __sgt_iter(sgl, use_dma(iobase)),
		.iobase = iobase,
	};
	unsigned long pfns[PSI915_REMAP_BATCH];
	unsigned long n = size >> PAGE_SHIFT, done = 0;
	int err = 0;

	GEM_BUG_ON((vma->vm_flags & PSI915_EXPECTED_FLAGS) != PSI915_EXPECTED_FLAGS);

	while (offset >= r.sgt.max >> PAGE_SHIFT) {
		offset -= r.sgt.max >> PAGE_SHIFT;
		r.sgt = __sgt_iter(__sg_next(r.sgt.sgp), use_dma(iobase));
		if (!r.sgt.sgp)
			return -EINVAL;
	}
	r.sgt.curr = offset << PAGE_SHIFT;

	while (done < n) {
		unsigned long k = 0;

		while (k < PSI915_REMAP_BATCH && done + k < n) {
			if (GEM_WARN_ON(!r.sgt.sgp)) {
				err = -EINVAL;
				break;
			}
			pfns[k++] = sgt_pfn(&r);
			r.sgt.curr += PAGE_SIZE;
			if (r.sgt.curr >= r.sgt.max)
				r.sgt = __sgt_iter(__sg_next(r.sgt.sgp),
						   use_dma(iobase));
		}
		if (k > 0 && !err)
			err = lkpi_vma_insert_pfns(vma, addr + (done << PAGE_SHIFT),
						   pfns, 0, k, r.prot);
		if (err)
			break;
		done += k;
	}
	if (unlikely(err) && done)
		zap_vma_ptes(vma, addr, done << PAGE_SHIFT);
	return err;
}
#else /* !CONFIG_PSI915: upstream page-table implementation */
static int remap_sg(pte_t *pte, unsigned long addr, void *data)
{
	struct remap_pfn *r = data;

	if (GEM_WARN_ON(!r->sgt.sgp))
		return -EINVAL;

	/* Special PTE are not associated with any struct page */
	set_pte_at(r->mm, addr, pte,
		   pte_mkspecial(pfn_pte(sgt_pfn(r), r->prot)));
	r->pfn++; /* track insertions in case we need to unwind later */

	r->sgt.curr += PAGE_SIZE;
	if (r->sgt.curr >= r->sgt.max)
		r->sgt = __sgt_iter(__sg_next(r->sgt.sgp), use_dma(r->iobase));

	return 0;
}

#define EXPECTED_FLAGS (VM_PFNMAP | VM_DONTEXPAND | VM_DONTDUMP)

#if IS_ENABLED(CONFIG_X86)
static int remap_pfn(pte_t *pte, unsigned long addr, void *data)
{
	struct remap_pfn *r = data;

	/* Special PTE are not associated with any struct page */
	set_pte_at(r->mm, addr, pte, pte_mkspecial(pfn_pte(r->pfn, r->prot)));
	r->pfn++;

	return 0;
}

/**
 * remap_io_mapping - remap an IO mapping to userspace
 * @vma: user vma to map to
 * @addr: target user address to start at
 * @pfn: physical address of kernel memory
 * @size: size of map area
 * @iomap: the source io_mapping
 *
 *  Note: this is only safe if the mm semaphore is held when called.
 */
int remap_io_mapping(struct vm_area_struct *vma,
		     unsigned long addr, unsigned long pfn, unsigned long size,
		     struct io_mapping *iomap)
{
	struct remap_pfn r;
	int err;

	GEM_BUG_ON((vma->vm_flags & EXPECTED_FLAGS) != EXPECTED_FLAGS);

	/* We rely on prevalidation of the io-mapping to skip track_pfn(). */
	r.mm = vma->vm_mm;
	r.pfn = pfn;
	r.prot = __pgprot((pgprot_val(iomap->prot) & _PAGE_CACHE_MASK) |
			  (pgprot_val(vma->vm_page_prot) & ~_PAGE_CACHE_MASK));

	err = apply_to_page_range(r.mm, addr, size, remap_pfn, &r);
	if (unlikely(err)) {
		zap_vma_ptes(vma, addr, (r.pfn - pfn) << PAGE_SHIFT);
		return err;
	}

	return 0;
}
#endif

/**
 * remap_io_sg - remap an IO mapping to userspace
 * @vma: user vma to map to
 * @addr: target user address to start at
 * @size: size of map area
 * @sgl: Start sg entry
 * @offset: offset from the start of the page
 * @iobase: Use stored dma address offset by this address or pfn if -1
 *
 *  Note: this is only safe if the mm semaphore is held when called.
 */
int remap_io_sg(struct vm_area_struct *vma,
		unsigned long addr, unsigned long size,
		struct scatterlist *sgl, unsigned long offset,
		resource_size_t iobase)
{
	struct remap_pfn r = {
		.mm = vma->vm_mm,
		.prot = vma->vm_page_prot,
		.sgt = __sgt_iter(sgl, use_dma(iobase)),
		.iobase = iobase,
	};
	int err;

	/* We rely on prevalidation of the io-mapping to skip track_pfn(). */
	GEM_BUG_ON((vma->vm_flags & EXPECTED_FLAGS) != EXPECTED_FLAGS);

	while (offset >= r.sgt.max >> PAGE_SHIFT) {
		offset -= r.sgt.max >> PAGE_SHIFT;
		r.sgt = __sgt_iter(__sg_next(r.sgt.sgp), use_dma(iobase));
		if (!r.sgt.sgp)
			return -EINVAL;
	}
	r.sgt.curr = offset << PAGE_SHIFT;

	if (!use_dma(iobase))
		flush_cache_range(vma, addr, size);

	err = apply_to_page_range(r.mm, addr, size, remap_sg, &r);
	if (unlikely(err)) {
		zap_vma_ptes(vma, addr, r.pfn << PAGE_SHIFT);
		return err;
	}

	return 0;
}
#endif /* CONFIG_PSI915 */
