/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * Linux VMA emulation over a devmap-like backend: mmap, fault, fork,
 * partial unmap, zap. See <drmglue/lkpi_mm.h> and docs/10, section 3,
 * which states the lifecycle and lock order this file implements.
 */

#include <linux/kernel.h>
#include <linux/bug.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/bitmap.h>
#include <linux/bitops.h>
#include <linux/printk.h>
#include <linux/mm_types.h>

/*
 * page_to_pfn()/put_page() as defined by the KPI's <linux/mm.h>. That
 * header does not compile yet (its block comment contains "*" "/" in
 * "set_pages_*" + "/set_memory_*", docs/10 R12), so the two definitions
 * are restated here; switch to #include <linux/mm.h> once it is fixed.
 */
void lkpi_page_release(struct page *head);

static inline unsigned long
lkpi_vma_page_pfn(struct page *p)
{
	return ((unsigned long)(p->lkpi_phys >> PAGE_SHIFT));
}

static inline void
lkpi_vma_put_page(struct page *p)
{
	struct page *h = p->lkpi_head != NULL ? p->lkpi_head : p;

	if (atomic_dec_and_test(&h->_refcount))
		lkpi_page_release(h);
}
#include <drmglue/lkpi_mm.h>
#include <drmglue/backend.h>
#include <drmglue/xlate.h>

/* Refault rounds before giving up on a range that keeps being zapped. */
#define	LKPI_VMA_FAULT_TRIES	16

pgprot_t
vm_get_page_prot(vm_flags_t flags)
{
	unsigned long p = 0;

	if (flags & VM_READ)
		p |= LKPI_PGPROT_READ;
	if (flags & VM_WRITE)
		p |= LKPI_PGPROT_WRITE;
	if (flags & VM_EXEC)
		p |= LKPI_PGPROT_EXEC;
	return (__pgprot(p | LKPI_CACHE_WB));
}

void
vma_set_file(struct vm_area_struct *vma, struct lkpi_file *file)
{
	struct lkpi_file *old = vma->vm_file;

	vma->vm_file = get_file(file);
	fput(old);
}

/* Unload every present window page in [first, first + n). lv->lock held. */
static void
lkpi_vma_win_unload_range(struct lkpi_vma *lv, unsigned long first,
    unsigned long n)
{
	unsigned long end = first + n, s, e;

	s = find_next_bit(lv->present, end, first);
	while (s < end) {
		e = find_next_zero_bit(lv->present, end, s);
		lkpi_glue_be->win_unload(lv->win, s, e - s);
		bitmap_clear(lv->present, s, e - s);
		s = find_next_bit(lv->present, end, e);
	}
}

/*
 * Zap [first, first + n) of lv: user translations first, then the window.
 * The user unload is unconditional, because a piece produced by a partial
 * munmap keeps user translations its (new, empty) window never had.
 * lv->lock held.
 */
static void
lkpi_vma_zap_locked(struct lkpi_vma *lv, unsigned long first, unsigned long n)
{
	if (lv->state == LKPI_VMA_DEAD || n == 0)
		return;
	if (lv->seg != NULL)
		lkpi_glue_be->user_unload(lv->seg,
		    lv->base_off + ((uint64_t)first << PAGE_SHIFT),
		    (size_t)n << PAGE_SHIFT);
	lkpi_vma_win_unload_range(lv, first, n);
	lv->zap_gen++;
}

static int
lkpi_vma_range(const struct lkpi_vma *lv, uint64_t off, size_t len,
    unsigned long *first, unsigned long *n)
{
	uint64_t last;

	if (len == 0 || off < lv->base_off)
		return (EINVAL);
	*first = (unsigned long)((off - lv->base_off) >> PAGE_SHIFT);
	last = (off + len - 1 - lv->base_off) >> PAGE_SHIFT;
	if (last >= lv->npages)
		return (EINVAL);
	*n = (unsigned long)last - *first + 1;
	return (0);
}

/*
 * Insertion (driver context: mmap handler or fault handler).
 */
int
lkpi_vma_insert_pfns(struct vm_area_struct *vma, unsigned long addr,
    const unsigned long *pfns, unsigned long pfn_base, unsigned long n,
    pgprot_t prot)
{
	struct lkpi_vma *lv = lkpi_vma_from_vma(vma);
	enum lkpi_cache_mode c = lkpi_pgprot_cache(prot);
	unsigned long first, i;
	int ret = 0;

	if ((addr & ~PAGE_MASK) != 0 || addr < vma->vm_start ||
	    addr >= vma->vm_end || n > (vma->vm_end - addr) >> PAGE_SHIFT)
		return (-EINVAL);
	first = (addr - vma->vm_start) >> PAGE_SHIFT;

	mutex_lock(&lv->lock);
	if (lv->state == LKPI_VMA_DEAD) {
		mutex_unlock(&lv->lock);
		return (-EFAULT);
	}
	if (lv->state == LKPI_VMA_NEW)
		lv->cache = lkpi_pgprot_cache(vma->vm_page_prot);
	if (c != lv->cache && !lv->attr_warned) {
		/*
		 * One cache attribute per devmap handle: the mapping's
		 * attribute wins (docs/10, section 3.4).
		 */
		lv->attr_warned = 1;
		pr_warn("drmglue: pfn insert with cache mode %d into a "
		    "mode %d mapping; using the mapping's\n", c, lv->cache);
	}
	for (i = 0; i < n; i++) {
		unsigned long pg = first + i;

		if (test_bit(pg, lv->present))
			continue;
		ret = lkpi_glue_be->win_load(lv->win, pg,
		    pfns != NULL ? pfns[i] : pfn_base + i, lv->cache);
		if (ret != 0) {
			ret = -EFAULT;
			break;
		}
		__set_bit(pg, lv->present);
	}
	mutex_unlock(&lv->lock);
	return (ret);
}

vm_fault_t
vmf_insert_pfn_prot(struct vm_area_struct *vma, unsigned long addr,
    unsigned long pfn, pgprot_t prot)
{
	int r = lkpi_vma_insert_pfns(vma, addr, NULL, pfn, 1, prot);

	if (r == 0)
		return (VM_FAULT_NOPAGE);
	return (r == -ENOMEM ? VM_FAULT_OOM : VM_FAULT_SIGBUS);
}

vm_fault_t
vmf_insert_pfn(struct vm_area_struct *vma, unsigned long addr,
    unsigned long pfn)
{
	return (vmf_insert_pfn_prot(vma, addr, pfn, vma->vm_page_prot));
}

int
remap_pfn_range(struct vm_area_struct *vma, unsigned long addr,
    unsigned long pfn, unsigned long size, pgprot_t prot)
{
	if ((size & ~PAGE_MASK) != 0)
		return (-EINVAL);
	return (lkpi_vma_insert_pfns(vma, addr, NULL, pfn, size >> PAGE_SHIFT,
	    prot));
}

int
vm_insert_page(struct vm_area_struct *vma, unsigned long addr,
    struct page *page)
{
	return (lkpi_vma_insert_pfns(vma, addr, NULL, lkpi_vma_page_pfn(page),
	    1, vma->vm_page_prot));
}

vm_fault_t
vmf_insert_page(struct vm_area_struct *vma, unsigned long addr,
    struct page *page)
{
	int r = vm_insert_page(vma, addr, page);

	if (r == 0)
		return (VM_FAULT_NOPAGE);
	return (r == -ENOMEM ? VM_FAULT_OOM : VM_FAULT_SIGBUS);
}

void
zap_vma_ptes(struct vm_area_struct *vma, unsigned long addr,
    unsigned long size)
{
	struct lkpi_vma *lv = lkpi_vma_from_vma(vma);
	unsigned long first, n;

	if (addr < vma->vm_start || addr + size > vma->vm_end || size == 0)
		return;
	first = (addr - vma->vm_start) >> PAGE_SHIFT;
	n = ((addr + size - 1 - vma->vm_start) >> PAGE_SHIFT) - first + 1;
	mutex_lock(&lv->lock);
	lkpi_vma_zap_locked(lv, first, n);
	mutex_unlock(&lv->lock);
}

void
unmap_mapping_range(struct address_space *mapping, loff_t holebegin,
    loff_t holelen, int even_cows)
{
	struct lkpi_itree_node *it;
	unsigned long a, b, s, e;
	struct lkpi_vma *lv;

	if (mapping == NULL || holebegin < 0)
		return;
	a = (unsigned long)((uint64_t)holebegin >> PAGE_SHIFT);
	if (holelen <= 0)
		b = ~0UL;
	else
		b = (unsigned long)(((uint64_t)holebegin + holelen - 1) >>
		    PAGE_SHIFT);

	down_read(&mapping->i_mmap_rwsem);
	for (it = lkpi_itree_first(&mapping->i_mmap, a, b); it != NULL;
	    it = lkpi_itree_next(it, a, b)) {
		lv = container_of(it, struct lkpi_vma, it);
		s = a > it->start ? a : it->start;
		e = b < it->last ? b : it->last;
		mutex_lock(&lv->lock);
		lkpi_vma_zap_locked(lv, s - it->start, e - s + 1);
		mutex_unlock(&lv->lock);
	}
	up_read(&mapping->i_mmap_rwsem);
}

/*
 * Lifecycle
 */
static struct lkpi_vma *
lkpi_vma_alloc(unsigned long npages)
{
	struct lkpi_vma *lv;

	if (npages == 0 || npages > (unsigned long)UINT_MAX)
		return (NULL);
	lv = kzalloc(sizeof (*lv), GFP_KERNEL);
	if (lv == NULL)
		return (NULL);
	lv->present = bitmap_zalloc((unsigned int)npages, GFP_KERNEL);
	if (lv->present == NULL) {
		kfree(lv);
		return (NULL);
	}
	lv->npages = npages;
	mutex_init(&lv->lock);
	if (lkpi_glue_be->win_create(lv, &lv->win) != 0) {
		mutex_destroy(&lv->lock);
		bitmap_free(lv->present);
		kfree(lv);
		return (NULL);
	}
	return (lv);
}

/* Free a VMA with no registration, no driver state and no file ref. */
static void
lkpi_vma_free(struct lkpi_vma *lv)
{
	mutex_lock(&lv->lock);
	lv->state = LKPI_VMA_DEAD;
	lkpi_vma_win_unload_range(lv, 0, lv->npages);
	mutex_unlock(&lv->lock);
	lkpi_glue_be->win_destroy(lv, lv->win);
	mutex_destroy(&lv->lock);
	bitmap_free(lv->present);
	kfree(lv);
}

static void
lkpi_vma_register(struct lkpi_vma *lv)
{
	struct address_space *m;

	m = lv->vma.vm_file != NULL ? lv->vma.vm_file->f_mapping : NULL;
	if (m == NULL)
		return;
	lv->it.start = lv->vma.vm_pgoff;
	lv->it.last = lv->vma.vm_pgoff + lv->npages - 1;
	down_write(&m->i_mmap_rwsem);
	lkpi_itree_insert(&m->i_mmap, &lv->it);
	lv->mapping = m;
	up_write(&m->i_mmap_rwsem);
}

int
lkpi_vma_create(struct lkpi_file *filp, uint64_t off, size_t len,
    unsigned int prot, unsigned int maxprot, unsigned int mflags,
    unsigned long uvaddr, struct lkpi_vma **lvp)
{
	struct lkpi_vma *lv;
	vm_flags_t vf;
	int r;

	*lvp = NULL;
	if ((mflags & LKPI_MAP_SHARED) == 0)
		return (EINVAL);	/* GEM and dma-buf are MAP_SHARED only */
	if (len == 0 || (off & ~(uint64_t)PAGE_MASK) != 0)
		return (EINVAL);
	len = (len + PAGE_SIZE - 1) & PAGE_MASK;
	if (filp->f_op->mmap == NULL)
		return (ENODEV);
	lv = lkpi_vma_alloc(len >> PAGE_SHIFT);
	if (lv == NULL)
		return (ENOMEM);

	vf = VM_SHARED | VM_MAYSHARE;
	if (prot & LKPI_PROT_READ)
		vf |= VM_READ;
	if (prot & LKPI_PROT_WRITE)
		vf |= VM_WRITE;
	if (prot & LKPI_PROT_EXEC)
		vf |= VM_EXEC;
	if (maxprot & LKPI_PROT_READ)
		vf |= VM_MAYREAD;
	if (maxprot & LKPI_PROT_WRITE)
		vf |= VM_MAYWRITE;
	if (maxprot & LKPI_PROT_EXEC)
		vf |= VM_MAYEXEC;
	lv->vma.vm_start = uvaddr;
	lv->vma.vm_end = uvaddr + len;
	lv->vma.vm_pgoff = (unsigned long)(off >> PAGE_SHIFT);
	vm_flags_init(&lv->vma, vf);
	lv->vma.vm_page_prot = vm_get_page_prot(vf);
	lv->vma.vm_file = get_file(filp);
	lv->base_off = off;
	lv->state = LKPI_VMA_NEW;
	lv->cache = LKPI_CACHE_WB;

	r = filp->f_op->mmap(filp, &lv->vma);
	if (r != 0) {
		struct lkpi_file *vf_file = lv->vma.vm_file;

		lkpi_vma_free(lv);
		fput(vf_file);
		return (lkpi_errno_to_user(r));
	}

	mutex_lock(&lv->lock);
	if (!bitmap_empty(lv->present, (unsigned int)lv->npages) &&
	    lv->cache != lkpi_pgprot_cache(lv->vma.vm_page_prot))
		pr_warn("drmglue: mmap handler changed cache mode after "
		    "inserting pages\n");
	lv->cache = lkpi_pgprot_cache(lv->vma.vm_page_prot);
	mutex_unlock(&lv->lock);

	lkpi_vma_register(lv);
	mutex_lock(&lv->lock);
	lv->state = LKPI_VMA_LIVE;
	mutex_unlock(&lv->lock);
	*lvp = lv;
	return (0);
}

void
lkpi_vma_bind(struct lkpi_vma *lv, void *seg)
{
	mutex_lock(&lv->lock);
	lv->seg = seg;
	mutex_unlock(&lv->lock);
}

int
lkpi_vma_fault(struct lkpi_vma *lv, uint64_t off, size_t len, int type,
    bool write)
{
	const struct vm_operations_struct *ops = lv->vma.vm_ops;
	unsigned long first, n, end, p;
	struct vm_fault vmf;
	unsigned int tries;
	vm_fault_t ret;
	int r;

	if (lkpi_vma_range(lv, off, len, &first, &n) != 0)
		return (EFAULT);
	end = first + n;

	for (tries = 0; ; tries++) {
		mutex_lock(&lv->lock);
		if (lv->state != LKPI_VMA_LIVE) {
			mutex_unlock(&lv->lock);
			return (EFAULT);
		}
		p = find_next_zero_bit(lv->present, end, first);
		if (p >= end) {
			/*
			 * Everything present: copy the window translations
			 * to the user while holding lv->lock, so a zap
			 * cannot unload the window in between (it would
			 * leave PFN_INVALID for devmap_load).
			 */
			r = lkpi_glue_be->user_load(lv->seg, off, len, type,
			    write);
			mutex_unlock(&lv->lock);
			return (r);
		}
		mutex_unlock(&lv->lock);

		if (ops == NULL || ops->fault == NULL)
			return (EFAULT);
		if (tries >= LKPI_VMA_FAULT_TRIES) {
			/*
			 * Livelock guard. An ordinary fault returns without
			 * loading anything: the access refaults, exactly as
			 * a Linux fault that installed no PTE. F_SOFTLOCK
			 * needs the translations, so it fails.
			 */
			return (type == LKPI_FAULT_SOFTLOCK ? EAGAIN : 0);
		}

		while (p < end) {
			memset(&vmf, 0, sizeof (vmf));
			vmf.vma = &lv->vma;
			vmf.flags = FAULT_FLAG_USER | FAULT_FLAG_ALLOW_RETRY |
			    FAULT_FLAG_KILLABLE | (write ? FAULT_FLAG_WRITE : 0);
			if (tries > 0)
				vmf.flags |= FAULT_FLAG_TRIED;
			vmf.gfp_mask = GFP_KERNEL;
			vmf.pgoff = lv->vma.vm_pgoff + p;
			vmf.address = lv->vma.vm_start + (p << PAGE_SHIFT);

			ret = ops->fault(&vmf);
			if (ret & VM_FAULT_ERROR)
				return ((ret & VM_FAULT_OOM) ? ENOMEM : EFAULT);
			if (ret & VM_FAULT_RETRY)
				break;
			if (!(ret & VM_FAULT_NOPAGE) && vmf.page != NULL) {
				r = lkpi_vma_insert_pfns(&lv->vma,
				    vmf.address, NULL,
				    lkpi_vma_page_pfn(vmf.page), 1,
				    lv->vma.vm_page_prot);
				lkpi_vma_put_page(vmf.page);
				if (r != 0)
					return (EFAULT);
			}
			mutex_lock(&lv->lock);
			p = find_next_zero_bit(lv->present, end, p + 1);
			mutex_unlock(&lv->lock);
		}
	}
}

/* New VMA for [poff, poff + plen) of lv, bound to seg (fork or split). */
static int
lkpi_vma_clone_range(struct lkpi_vma *lv, uint64_t poff, size_t plen,
    void *seg, struct lkpi_vma **nlvp)
{
	unsigned long skip = (unsigned long)((poff - lv->base_off) >>
	    PAGE_SHIFT);
	struct lkpi_vma *nlv;

	*nlvp = NULL;
	nlv = lkpi_vma_alloc(plen >> PAGE_SHIFT);
	if (nlv == NULL)
		return (ENOMEM);
	memcpy(&nlv->vma, &lv->vma, sizeof (nlv->vma));
	nlv->vma.vm_start = lv->vma.vm_start + (skip << PAGE_SHIFT);
	nlv->vma.vm_end = nlv->vma.vm_start + plen;
	nlv->vma.vm_pgoff = lv->vma.vm_pgoff + skip;
	nlv->vma.vm_file = get_file(lv->vma.vm_file);
	nlv->base_off = poff;
	nlv->cache = lv->cache;
	nlv->seg = seg;
	nlv->state = LKPI_VMA_LIVE;
	if (nlv->vma.vm_ops != NULL && nlv->vma.vm_ops->open != NULL)
		nlv->vma.vm_ops->open(&nlv->vma);
	lkpi_vma_register(nlv);
	*nlvp = nlv;
	return (0);
}

int
lkpi_vma_dup(struct lkpi_vma *lv, void *new_seg, struct lkpi_vma **nlvp)
{
	return (lkpi_vma_clone_range(lv, lv->base_off,
	    lv->npages << PAGE_SHIFT, new_seg, nlvp));
}

void
lkpi_vma_destroy(struct lkpi_vma *lv)
{
	struct address_space *m = lv->mapping;
	struct lkpi_file *f;

	/* 1. No new zapper can find it; running zappers are drained. */
	if (m != NULL) {
		down_write(&m->i_mmap_rwsem);
		lkpi_itree_remove(&m->i_mmap, &lv->it);
		lv->mapping = NULL;
		up_write(&m->i_mmap_rwsem);
	}
	/* 2. Faults and insertions fail from now on. */
	mutex_lock(&lv->lock);
	lv->state = LKPI_VMA_DEAD;
	lkpi_vma_win_unload_range(lv, 0, lv->npages);
	mutex_unlock(&lv->lock);
	/* 3. Driver state (drops the object reference), no glue locks held. */
	if (lv->vma.vm_ops != NULL && lv->vma.vm_ops->close != NULL)
		lv->vma.vm_ops->close(&lv->vma);
	f = lv->vma.vm_file;
	lkpi_glue_be->win_destroy(lv, lv->win);
	mutex_destroy(&lv->lock);
	bitmap_free(lv->present);
	kfree(lv);
	/* 4. Last: may run f_op->release and anything it triggers. */
	fput(f);
}

void
lkpi_vma_unmap(struct lkpi_vma *lv, uint64_t off, size_t len,
    void *new_seg1, struct lkpi_vma **nlv1, void *new_seg2,
    struct lkpi_vma **nlv2)
{
	uint64_t end = lv->base_off + ((uint64_t)lv->npages << PAGE_SHIFT);
	uint64_t hole_end = off + ((len + PAGE_SIZE - 1) & PAGE_MASK);

	if (nlv1 != NULL)
		*nlv1 = NULL;
	if (nlv2 != NULL)
		*nlv2 = NULL;
	/*
	 * Surviving pieces get their own VMA (vm_ops->open, as Linux does on
	 * a split) before the original goes (vm_ops->close). On allocation
	 * failure the piece has no VMA: its later faults fail with EFAULT,
	 * which is the best available outcome inside an unmap callback.
	 */
	if (new_seg1 != NULL && nlv1 != NULL && off > lv->base_off)
		(void) lkpi_vma_clone_range(lv, lv->base_off,
		    (size_t)(off - lv->base_off), new_seg1, nlv1);
	if (new_seg2 != NULL && nlv2 != NULL && hole_end < end)
		(void) lkpi_vma_clone_range(lv, hole_end,
		    (size_t)(end - hole_end), new_seg2, nlv2);
	lkpi_vma_destroy(lv);
}
