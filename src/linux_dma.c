/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * DMA mapping API over the backend's ddi_dma bindings (see
 * <linux/dma-mapping.h>). Clean-room.
 *
 * Coherent allocations and single/page mappings are recorded in a hash
 * table keyed by (device, DMA address) [coherent: kernel address] so the
 * free/unmap/sync calls, which only pass addresses, find the binding.
 */

#include <linux/kernel.h>
#include <linux/dma-mapping.h>
#include <linux/hashtable.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/bug.h>
#include <linuxkpi/plat_dev.h>

struct lkpi_dma_rec {
	struct hlist_node	node;
	struct device		*dev;
	u64			key;	/* DMA address, or kva for coherent */
	dma_addr_t		addr;
	size_t			size;
	struct lkpi_plat_dma	*h;
	bool			coherent;
};

static DEFINE_HASHTABLE(lkpi_dma_hash, 8);
static DEFINE_SPINLOCK(lkpi_dma_lock);

static void
lkpi_dma_rec_add(struct lkpi_dma_rec *r)
{
	spin_lock(&lkpi_dma_lock);
	hash_add(lkpi_dma_hash, &r->node, r->key);
	spin_unlock(&lkpi_dma_lock);
}

static struct lkpi_dma_rec *
lkpi_dma_rec_find(struct device *dev, u64 key, bool coherent, bool remove)
{
	struct lkpi_dma_rec *r;

	spin_lock(&lkpi_dma_lock);
	hash_for_each_possible(lkpi_dma_hash, r, node, key) {
		if (r->dev == dev && r->key == key && r->coherent == coherent) {
			if (remove)
				hash_del(&r->node);
			spin_unlock(&lkpi_dma_lock);
			return (r);
		}
	}
	spin_unlock(&lkpi_dma_lock);
	return (NULL);
}

int
dma_set_mask(struct device *dev, u64 mask)
{
	if (dev->dma_mask == NULL)
		return (-EIO);
	*dev->dma_mask = mask;
	return (0);
}

int
dma_set_coherent_mask(struct device *dev, u64 mask)
{
	dev->coherent_dma_mask = mask;
	return (0);
}

static u64
lkpi_coherent_mask(struct device *dev)
{
	return (dev->coherent_dma_mask != 0 ? dev->coherent_dma_mask :
	    DMA_BIT_MASK(32));
}

static int
lkpi_dma_dir(enum dma_data_direction dir)
{
	switch (dir) {
	case DMA_TO_DEVICE:
		return (LKPI_DMA_TO_DEVICE);
	case DMA_FROM_DEVICE:
		return (LKPI_DMA_FROM_DEVICE);
	default:
		return (LKPI_DMA_BIDIR);
	}
}

void *
dma_alloc_attrs(struct device *dev, size_t size, dma_addr_t *dma, gfp_t gfp,
    unsigned long attrs)
{
	struct lkpi_dma_rec *r;
	void *kva;
	u64 bus;

	(void) attrs;
	r = kzalloc(sizeof (*r), gfp);
	if (r == NULL)
		return (NULL);
	if (lkpi_plat_dma_alloc_coherent(lkpi_dev_dip(dev),
	    lkpi_coherent_mask(dev), size, &kva, &bus, &r->h) != 0) {
		kfree(r);
		return (NULL);
	}
	r->dev = dev;
	r->key = (u64)(uintptr_t)kva;
	r->addr = bus;
	r->size = size;
	r->coherent = true;
	lkpi_dma_rec_add(r);
	*dma = bus;
	return (kva);
}

void
dma_free_attrs(struct device *dev, size_t size, void *vaddr, dma_addr_t dma,
    unsigned long attrs)
{
	struct lkpi_dma_rec *r;

	(void) attrs;
	if (vaddr == NULL)
		return;
	r = lkpi_dma_rec_find(dev, (u64)(uintptr_t)vaddr, true, true);
	if (r == NULL) {
		WARN(1, "linuxkpi: dma_free_coherent of unknown %p\n", vaddr);
		return;
	}
	WARN_ON(r->addr != dma || r->size != size);
	lkpi_plat_dma_free_coherent(r->h);
	kfree(r);
}

struct lkpi_dmam {
	void		*vaddr;
	dma_addr_t	dma;
	size_t		size;
	unsigned long	attrs;
};

static void
lkpi_dmam_release(struct device *dev, void *res)
{
	struct lkpi_dmam *m = res;

	dma_free_attrs(dev, m->size, m->vaddr, m->dma, m->attrs);
}

void *
dmam_alloc_attrs(struct device *dev, size_t size, dma_addr_t *dma, gfp_t gfp,
    unsigned long attrs)
{
	struct lkpi_dmam *m;

	m = devres_alloc(lkpi_dmam_release, sizeof (*m), gfp);
	if (m == NULL)
		return (NULL);
	m->vaddr = dma_alloc_attrs(dev, size, dma, gfp, attrs);
	if (m->vaddr == NULL) {
		devres_free(m);
		return (NULL);
	}
	m->dma = *dma;
	m->size = size;
	m->attrs = attrs;
	devres_add(dev, m);
	return (m->vaddr);
}

dma_addr_t
lkpi_dma_map_kva(struct device *dev, void *kva, size_t size,
    enum dma_data_direction dir, unsigned long attrs)
{
	struct lkpi_dma_rec *r;
	u64 bus;

	(void) attrs;
	r = kzalloc(sizeof (*r), GFP_KERNEL);
	if (r == NULL)
		return (DMA_MAPPING_ERROR);
	if (lkpi_plat_dma_map(lkpi_dev_dip(dev), dma_get_mask(dev), kva, size,
	    lkpi_dma_dir(dir), &bus, &r->h) != 0) {
		kfree(r);
		return (DMA_MAPPING_ERROR);
	}
	r->dev = dev;
	r->key = bus;
	r->addr = bus;
	r->size = size;
	lkpi_dma_rec_add(r);
	return (bus);
}

dma_addr_t
dma_map_page_attrs(struct device *dev, struct page *page, size_t offset,
    size_t size, enum dma_data_direction dir, unsigned long attrs)
{
	return (lkpi_dma_map_kva(dev, (char *)page_address(page) + offset,
	    size, dir, attrs));
}

void
dma_unmap_page_attrs(struct device *dev, dma_addr_t addr, size_t size,
    enum dma_data_direction dir, unsigned long attrs)
{
	struct lkpi_dma_rec *r;

	(void) attrs;
	r = lkpi_dma_rec_find(dev, addr, false, true);
	if (r == NULL) {
		WARN(1, "linuxkpi: dma_unmap of unknown %#llx\n",
		    (unsigned long long)addr);
		return;
	}
	WARN_ON(r->size != size);
	if (dir != DMA_TO_DEVICE)
		lkpi_plat_dma_sync(r->h, 0, r->size, false);
	lkpi_plat_dma_unmap(r->h);
	kfree(r);
}

static void
lkpi_dma_sync_single(struct device *dev, dma_addr_t addr, size_t size,
    bool for_device)
{
	struct lkpi_dma_rec *r = lkpi_dma_rec_find(dev, addr, false, false);

	if (r == NULL) {
		/* Linux allows syncing a sub-range: search containing maps. */
		struct lkpi_dma_rec *it;
		unsigned int bkt;

		spin_lock(&lkpi_dma_lock);
		hash_for_each(lkpi_dma_hash, bkt, it, node) {
			if (it->dev == dev && !it->coherent &&
			    addr >= it->addr && addr + size <= it->addr +
			    it->size) {
				r = it;
				break;
			}
		}
		spin_unlock(&lkpi_dma_lock);
		if (r == NULL)
			return;
	}
	lkpi_plat_dma_sync(r->h, (size_t)(addr - r->addr), size, for_device);
}

void
dma_sync_single_for_cpu(struct device *dev, dma_addr_t addr, size_t size,
    enum dma_data_direction dir)
{
	if (dir != DMA_TO_DEVICE)
		lkpi_dma_sync_single(dev, addr, size, false);
}

void
dma_sync_single_for_device(struct device *dev, dma_addr_t addr, size_t size,
    enum dma_data_direction dir)
{
	if (dir != DMA_FROM_DEVICE)
		lkpi_dma_sync_single(dev, addr, size, true);
}

/*
 * dma_map_sg: bind each entry by its PAGES, not by sg_virt().
 *
 * An entry may cover several pages that are physically consecutive but
 * were allocated separately (alloc_page() each; sg_alloc_table_from_pages
 * and i915 merge them), so their kernel VAs are unrelated: binding the VA
 * range [sg_virt(sg), +length) would hand the device foreign memory
 * (docs/12 M1). The entry is walked by PFN: when its pages are also
 * VA-contiguous (one allocation, or memory without struct pages such as
 * kmalloc buffers) the VA range is bound; otherwise the page list is
 * bound with lkpi_plat_dma_map_pages() (illumos: a B_SHADOW buf whose
 * page_t list gives each page's PFN). Either way one handle and one bus
 * segment per entry.
 */
struct lkpi_sg_dma {
	struct lkpi_plat_dma	*h;
	unsigned int		len;
};

static void
lkpi_sg_dma_free(struct lkpi_sg_dma *d)
{
	lkpi_plat_dma_unmap(d->h);
	kfree(d);
}

static void
lkpi_sg_dma_sync(struct lkpi_sg_dma *d, bool for_device)
{
	lkpi_plat_dma_sync(d->h, 0, d->len, for_device);
}

static struct lkpi_sg_dma *
lkpi_sg_dma_map(struct device *dev, struct scatterlist *sg, int dir,
    u64 *busp)
{
	void *dip = lkpi_dev_dip(dev);
	u64 mask = dma_get_mask(dev);
	phys_addr_t phys = sg_phys(sg);
	unsigned int off = (unsigned int)(phys & (PAGE_SIZE - 1));
	size_t npg = (off + sg->length + PAGE_SIZE - 1) >> PAGE_SHIFT, i;
	struct lkpi_sg_dma *d;
	void **kvas = NULL;
	u64 *pas = NULL;
	bool contig = true;
	int r;

	d = kzalloc(sizeof (*d), GFP_KERNEL);
	if (d == NULL)
		return (NULL);
	d->len = sg->length;
	if (npg > 1) {
		kvas = kmalloc_array(npg, sizeof (*kvas), GFP_KERNEL);
		pas = kmalloc_array(npg, sizeof (*pas), GFP_KERNEL);
		if (kvas == NULL || pas == NULL)
			goto fail;
		for (i = 0; i < npg; i++) {
			u64 pa = (phys & ~(u64)(PAGE_SIZE - 1)) + i * PAGE_SIZE;
			struct page *pg = pfn_to_page((unsigned long)
			    (pa >> PAGE_SHIFT));

			if (pg == NULL) {
				/* No struct page (kmalloc): VA-contiguous. */
				contig = true;
				break;
			}
			kvas[i] = page_address(pg);
			pas[i] = pa;
			if (i > 0 && (char *)kvas[i] != (char *)kvas[0] +
			    i * PAGE_SIZE)
				contig = false;
		}
	}
	if (contig)
		r = lkpi_plat_dma_map(dip, mask, sg_virt(sg), sg->length, dir,
		    busp, &d->h);
	else
		r = lkpi_plat_dma_map_pages(dip, mask, kvas, pas, npg, off,
		    sg->length, dir, busp, &d->h);
	kfree(kvas);
	kfree(pas);
	if (r != 0) {
		kfree(d);
		return (NULL);
	}
	return (d);
fail:
	kfree(kvas);
	kfree(pas);
	kfree(d);
	return (NULL);
}

int
dma_map_sg_attrs(struct device *dev, struct scatterlist *sgl, int nents,
    enum dma_data_direction dir, unsigned long attrs)
{
	struct scatterlist *sg;
	u64 bus = 0;
	int i, j;

	(void) attrs;
	for_each_sg(sgl, sg, nents, i) {
		sg->lkpi_dma = lkpi_sg_dma_map(dev, sg, lkpi_dma_dir(dir), &bus);
		if (sg->lkpi_dma == NULL) {
			struct scatterlist *s2;

			for_each_sg(sgl, s2, i, j) {
				lkpi_sg_dma_free(s2->lkpi_dma);
				s2->lkpi_dma = NULL;
			}
			return (0);
		}
		sg->dma_address = bus;
		sg->dma_length = sg->length;
	}
	return (nents);
}

void
dma_unmap_sg_attrs(struct device *dev, struct scatterlist *sgl, int nents,
    enum dma_data_direction dir, unsigned long attrs)
{
	struct scatterlist *sg;
	int i;

	(void) dev;
	(void) attrs;
	for_each_sg(sgl, sg, nents, i) {
		if (sg->lkpi_dma == NULL)
			continue;
		if (dir != DMA_TO_DEVICE)
			lkpi_sg_dma_sync(sg->lkpi_dma, false);
		lkpi_sg_dma_free(sg->lkpi_dma);
		sg->lkpi_dma = NULL;
		sg->dma_address = 0;
		sg->dma_length = 0;
	}
}

void
dma_sync_sg_for_cpu(struct device *dev, struct scatterlist *sgl, int nents,
    enum dma_data_direction dir)
{
	struct scatterlist *sg;
	int i;

	(void) dev;
	if (dir == DMA_TO_DEVICE)
		return;
	for_each_sg(sgl, sg, nents, i)
		if (sg->lkpi_dma != NULL)
			lkpi_sg_dma_sync(sg->lkpi_dma, false);
}

void
dma_sync_sg_for_device(struct device *dev, struct scatterlist *sgl,
    int nents, enum dma_data_direction dir)
{
	struct scatterlist *sg;
	int i;

	(void) dev;
	if (dir == DMA_FROM_DEVICE)
		return;
	for_each_sg(sgl, sg, nents, i)
		if (sg->lkpi_dma != NULL)
			lkpi_sg_dma_sync(sg->lkpi_dma, true);
}

/* IOMMU translation of the device's DMA (i915 intel_vtd_active()). */
bool
device_iommu_mapped(struct device *dev)
{
	void *dip = lkpi_dev_dip(dev);

	return (dip != NULL && lkpi_plat_dev_iommu_mapped(dip));
}
