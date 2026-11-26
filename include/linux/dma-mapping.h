/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation of the Linux DMA API from its documented
 * behaviour (Documentation/core-api/dma-api.rst); function set
 * cross-checked against FreeBSD's LinuxKPI dma-mapping.h (BSD-2-Clause).
 *
 * Mappings are illumos ddi_dma bindings (src/linux_dma.c over
 * <linuxkpi/plat_dev.h>) made on the dev_info_t of the device (or its
 * nearest ancestor), with dma_attr_addr_hi from the device's DMA mask
 * and dma_attr_sgllen = 1, so every mapping is one bus-contiguous segment
 * and the returned dma_addr_t is the cookie address: IOMMU-translated
 * when the illumos IOMMU (immu) is active, never assumed to equal the
 * physical address.
 *
 *   dma_alloc_coherent()   ddi_dma_mem_alloc(DDI_DMA_CONSISTENT) + bind
 *   dma_map_page/single()  bind of the kernel range (DDI_DMA_STREAMING);
 *                          the range must be bus-contiguous (one page,
 *                          memory from alloc_pages(), or any memory with
 *                          the IOMMU on), else DMA_MAPPING_ERROR
 *   dma_map_sg()           one binding per entry; entries are not merged,
 *                          so the return value equals nents
 *   dma_sync_*()           ddi_dma_sync(9F)
 *
 * Bindings are recorded (by device and DMA address for single/page, in
 * the entry for sg) so unmap finds them. Calls may be made from
 * interrupt handlers below LOCK_LEVEL only when they do not sleep
 * (GFP_ATOMIC for dma_alloc_coherent; map/unmap may sleep allocating a
 * handle and must not be called from interrupt context).
 */
#ifndef _LINUXKPI_LINUX_DMA_MAPPING_H_
#define	_LINUXKPI_LINUX_DMA_MAPPING_H_

#include <linuxkpi/platform.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/dma-direction.h>

#define	DMA_BIT_MASK(n)		(((n) == 64) ? ~0ULL : ((1ULL << (n)) - 1))
#define	DMA_MAPPING_ERROR	(~(dma_addr_t)0)

#define	DMA_ATTR_WEAK_ORDERING		(1UL << 1)
#define	DMA_ATTR_WRITE_COMBINE		(1UL << 2)
#define	DMA_ATTR_NO_KERNEL_MAPPING	(1UL << 4)
#define	DMA_ATTR_SKIP_CPU_SYNC		(1UL << 5)
#define	DMA_ATTR_FORCE_CONTIGUOUS	(1UL << 6)
#define	DMA_ATTR_ALLOC_SINGLE_PAGES	(1UL << 7)
#define	DMA_ATTR_NO_WARN		(1UL << 8)
#define	DMA_ATTR_PRIVILEGED		(1UL << 9)

int dma_set_mask(struct device *dev, u64 mask);
int dma_set_coherent_mask(struct device *dev, u64 mask);

static inline int
dma_set_mask_and_coherent(struct device *dev, u64 mask)
{
	int r = dma_set_mask(dev, mask);

	if (r == 0)
		r = dma_set_coherent_mask(dev, mask);
	return (r);
}

static inline u64
dma_get_mask(struct device *dev)
{
	if (dev->dma_mask != NULL && *dev->dma_mask != 0)
		return (*dev->dma_mask);
	return (DMA_BIT_MASK(32));
}

static inline int
dma_coerce_mask_and_coherent(struct device *dev, u64 mask)
{
	dev->dma_mask = &dev->coherent_dma_mask;
	return (dma_set_mask_and_coherent(dev, mask));
}

static inline int
dma_mapping_error(struct device *dev, dma_addr_t addr)
{
	(void) dev;
	return (addr == DMA_MAPPING_ERROR ? -ENOMEM : 0);
}

void *dma_alloc_attrs(struct device *dev, size_t size, dma_addr_t *dma,
    gfp_t gfp, unsigned long attrs);
void dma_free_attrs(struct device *dev, size_t size, void *vaddr,
    dma_addr_t dma, unsigned long attrs);
dma_addr_t dma_map_page_attrs(struct device *dev, struct page *page,
    size_t offset, size_t size, enum dma_data_direction dir,
    unsigned long attrs);
dma_addr_t lkpi_dma_map_kva(struct device *dev, void *kva, size_t size,
    enum dma_data_direction dir, unsigned long attrs);
void dma_unmap_page_attrs(struct device *dev, dma_addr_t addr, size_t size,
    enum dma_data_direction dir, unsigned long attrs);
int dma_map_sg_attrs(struct device *dev, struct scatterlist *sg, int nents,
    enum dma_data_direction dir, unsigned long attrs);
void dma_unmap_sg_attrs(struct device *dev, struct scatterlist *sg,
    int nents, enum dma_data_direction dir, unsigned long attrs);
void dma_sync_single_for_cpu(struct device *dev, dma_addr_t addr,
    size_t size, enum dma_data_direction dir);
void dma_sync_single_for_device(struct device *dev, dma_addr_t addr,
    size_t size, enum dma_data_direction dir);
void dma_sync_sg_for_cpu(struct device *dev, struct scatterlist *sg,
    int nents, enum dma_data_direction dir);
void dma_sync_sg_for_device(struct device *dev, struct scatterlist *sg,
    int nents, enum dma_data_direction dir);

#define	dma_alloc_coherent(d, s, h, g)	dma_alloc_attrs(d, s, h, g, 0)
#define	dma_free_coherent(d, s, v, h)	dma_free_attrs(d, s, v, h, 0)
#define	dma_alloc_wc(d, s, h, g)					\
	dma_alloc_attrs(d, s, h, g, DMA_ATTR_WRITE_COMBINE)
#define	dma_free_wc(d, s, v, h)						\
	dma_free_attrs(d, s, v, h, DMA_ATTR_WRITE_COMBINE)
#define	dma_map_page(d, p, o, s, r)	dma_map_page_attrs(d, p, o, s, r, 0)
#define	dma_unmap_page(d, a, s, r)	dma_unmap_page_attrs(d, a, s, r, 0)
#define	dma_map_single(d, p, s, r)	lkpi_dma_map_kva(d, p, s, r, 0)
#define	dma_map_single_attrs(d, p, s, r, a) lkpi_dma_map_kva(d, p, s, r, a)
#define	dma_unmap_single(d, a, s, r)	dma_unmap_page_attrs(d, a, s, r, 0)
#define	dma_unmap_single_attrs(d, a, s, r, at) dma_unmap_page_attrs(d, a, s, r, at)
#define	dma_map_sg(d, s, n, r)		dma_map_sg_attrs(d, s, n, r, 0)
#define	dma_unmap_sg(d, s, n, r)	dma_unmap_sg_attrs(d, s, n, r, 0)
#define	dma_map_resource(d, p, s, r, a)	DMA_MAPPING_ERROR
#define	dma_unmap_resource(d, a, s, r, at) do { } while (0)

void *dmam_alloc_attrs(struct device *dev, size_t size, dma_addr_t *dma,
    gfp_t gfp, unsigned long attrs);
#define	dmam_alloc_coherent(d, s, h, g)	dmam_alloc_attrs(d, s, h, g, 0)

static inline int
dma_map_sgtable(struct device *dev, struct sg_table *sgt,
    enum dma_data_direction dir, unsigned long attrs)
{
	int n = dma_map_sg_attrs(dev, sgt->sgl, (int)sgt->orig_nents, dir,
	    attrs);

	if (n <= 0)
		return (-ENOMEM);
	sgt->nents = (unsigned int)n;
	return (0);
}

static inline void
dma_unmap_sgtable(struct device *dev, struct sg_table *sgt,
    enum dma_data_direction dir, unsigned long attrs)
{
	dma_unmap_sg_attrs(dev, sgt->sgl, (int)sgt->orig_nents, dir, attrs);
}

#define	dma_sync_sgtable_for_cpu(d, t, r)				\
	dma_sync_sg_for_cpu(d, (t)->sgl, (int)(t)->orig_nents, r)
#define	dma_sync_sgtable_for_device(d, t, r)				\
	dma_sync_sg_for_device(d, (t)->sgl, (int)(t)->orig_nents, r)

static inline size_t
dma_max_mapping_size(struct device *dev)
{
	(void) dev;
	return (SIZE_MAX);
}

static inline unsigned int
dma_get_max_seg_size(struct device *dev)
{
	if (dev->dma_parms != NULL && dev->dma_parms->max_segment_size != 0)
		return (dev->dma_parms->max_segment_size);
	return (SCATTERLIST_MAX_SEGMENT);
}

static inline int
dma_set_max_seg_size(struct device *dev, unsigned int size)
{
	if (dev->dma_parms == NULL)
		return (-EIO);
	dev->dma_parms->max_segment_size = size;
	return (0);
}

#define	dma_addressing_limited(dev)	(dma_get_mask(dev) < DMA_BIT_MASK(64))
#define	dma_set_seg_boundary(d, m)	((void)(d), 0)
#define	dma_get_cache_alignment()	(64)
#define	dma_is_dma_buf(x)		(false)
#define	dma_need_sync(d, a)		(true)

#endif /* _LINUXKPI_LINUX_DMA_MAPPING_H_ */
