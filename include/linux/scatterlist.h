/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation of the Linux scatterlist API from its
 * documented behaviour; function set cross-checked against FreeBSD's
 * LinuxKPI scatterlist.h (BSD-2-Clause).
 *
 * page_link holds the struct page pointer with bit 0 = chain entry and
 * bit 1 = last entry, as in Linux, so sg_next()/sg_chain() and the
 * iterators behave identically. sg_alloc_table() allocates one array
 * (kvcalloc) instead of chained chunks; chains built by callers with
 * sg_chain() are still followed. Each mapped entry keeps its DMA binding
 * in lkpi_dma (src/linux_dma.c).
 */
#ifndef _LINUXKPI_LINUX_SCATTERLIST_H_
#define	_LINUXKPI_LINUX_SCATTERLIST_H_

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/bug.h>
#include <linux/mm.h>
#include <linux/gfp.h>

struct lkpi_plat_dma;

struct scatterlist {
	unsigned long		page_link;
	unsigned int		offset;
	unsigned int		length;
	dma_addr_t		dma_address;
	unsigned int		dma_length;
	struct lkpi_sg_dma	*lkpi_dma;	/* dma_map_sg bindings */
};

struct lkpi_sg_dma;

struct sg_table {
	struct scatterlist	*sgl;
	unsigned int		nents;
	unsigned int		orig_nents;
};

struct sg_append_table {
	struct sg_table		sgt;
	struct scatterlist	*prv;
	unsigned int		total_nents;
};

#define	SG_CHAIN		0x01UL
#define	SG_END			0x02UL
#define	SG_PAGE_LINK_MASK	(SG_CHAIN | SG_END)
#define	SG_MAX_SINGLE_ALLOC	(PAGE_SIZE / sizeof (struct scatterlist))
#define	SCATTERLIST_MAX_SEGMENT	(UINT_MAX & PAGE_MASK)

#define	sg_is_chain(sg)		(((sg)->page_link & SG_CHAIN) != 0)
#define	sg_is_last(sg)		(((sg)->page_link & SG_END) != 0)
#define	sg_chain_ptr(sg)						\
	((struct scatterlist *)((sg)->page_link & ~SG_PAGE_LINK_MASK))
#define	sg_dma_address(sg)	((sg)->dma_address)
#define	sg_dma_len(sg)		((sg)->dma_length)
#define	sg_dma_is_bus_address(sg) (false)

static inline struct page *
sg_page(struct scatterlist *sg)
{
	return ((struct page *)(sg->page_link & ~SG_PAGE_LINK_MASK));
}

static inline void
sg_assign_page(struct scatterlist *sg, struct page *page)
{
	sg->page_link = (sg->page_link & SG_PAGE_LINK_MASK) |
	    (unsigned long)page;
}

static inline void
sg_set_page(struct scatterlist *sg, struct page *page, unsigned int len,
    unsigned int offset)
{
	sg_assign_page(sg, page);
	sg->offset = offset;
	sg->length = len;
}

static inline void *
sg_virt(struct scatterlist *sg)
{
	return ((char *)page_address(sg_page(sg)) + sg->offset);
}

static inline dma_addr_t
sg_phys(struct scatterlist *sg)
{
	return (page_to_phys(sg_page(sg)) + sg->offset);
}

static inline void
sg_mark_end(struct scatterlist *sg)
{
	sg->page_link |= SG_END;
	sg->page_link &= ~SG_CHAIN;
}

static inline void
sg_unmark_end(struct scatterlist *sg)
{
	sg->page_link &= ~SG_END;
}

static inline void
sg_init_table(struct scatterlist *sgl, unsigned int nents)
{
	memset(sgl, 0, sizeof (*sgl) * nents);
	sg_mark_end(&sgl[nents - 1]);
}

static inline void
sg_init_one(struct scatterlist *sg, const void *buf, unsigned int len)
{
	sg_init_table(sg, 1);
	sg_set_page(sg, virt_to_page((void *)((uintptr_t)buf & PAGE_MASK)),
	    len, (unsigned int)offset_in_page(buf));
}

static inline void
sg_set_buf(struct scatterlist *sg, const void *buf, unsigned int len)
{
	sg_set_page(sg, virt_to_page((void *)((uintptr_t)buf & PAGE_MASK)),
	    len, (unsigned int)offset_in_page(buf));
}

/* Make the last entry of prv (of prv_nents) point to sgl. */
static inline void
sg_chain(struct scatterlist *prv, unsigned int prv_nents,
    struct scatterlist *sgl)
{
	prv[prv_nents - 1].offset = 0;
	prv[prv_nents - 1].length = 0;
	prv[prv_nents - 1].page_link = ((unsigned long)sgl | SG_CHAIN) &
	    ~SG_END;
}

static inline struct scatterlist *
__lkpi_sg_next(struct scatterlist *sg)
{
	sg++;
	if (unlikely(sg_is_chain(sg)))
		sg = sg_chain_ptr(sg);
	return (sg);
}

static inline struct scatterlist *
sg_next(struct scatterlist *sg)
{
	if (sg_is_last(sg))
		return (NULL);
	return (__lkpi_sg_next(sg));
}

#define	for_each_sg(sglist, sg, nr, i)					\
	for ((i) = 0, (sg) = (sglist); (i) < (nr); (i)++, (sg) = sg_next(sg))
#define	for_each_sgtable_sg(sgt, sg, i)					\
	for_each_sg((sgt)->sgl, sg, (sgt)->orig_nents, i)
#define	for_each_sgtable_dma_sg(sgt, sg, i)				\
	for_each_sg((sgt)->sgl, sg, (sgt)->nents, i)

int sg_nents(struct scatterlist *sg);
int sg_alloc_table(struct sg_table *table, unsigned int nents, gfp_t gfp);
void sg_free_table(struct sg_table *table);
int sg_alloc_table_from_pages_segment(struct sg_table *sgt,
    struct page **pages, unsigned int n_pages, unsigned int offset,
    unsigned long size, unsigned int max_segment, gfp_t gfp);
#define	sg_free_append_table(t)	sg_free_table(&(t)->sgt)

static inline int
sg_alloc_table_from_pages(struct sg_table *sgt, struct page **pages,
    unsigned int n_pages, unsigned int offset, unsigned long size,
    gfp_t gfp)
{
	return (sg_alloc_table_from_pages_segment(sgt, pages, n_pages, offset,
	    size, SCATTERLIST_MAX_SEGMENT, gfp));
}

size_t sg_copy_from_buffer(struct scatterlist *sgl, unsigned int nents,
    const void *buf, size_t buflen);
size_t sg_copy_to_buffer(struct scatterlist *sgl, unsigned int nents,
    void *buf, size_t buflen);

/* Page iterators. */
struct sg_page_iter {
	struct scatterlist	*sg;
	unsigned int		sg_pgoffset;
	unsigned int		__nents;
	int			__pg_advance;
};

struct sg_dma_page_iter {
	struct sg_page_iter	base;
};

void __sg_page_iter_start(struct sg_page_iter *piter, struct scatterlist *sgl,
    unsigned int nents, unsigned long pgoffset);
bool __sg_page_iter_next(struct sg_page_iter *piter);
bool __sg_page_iter_dma_next(struct sg_dma_page_iter *dma_iter);

static inline struct page *
sg_page_iter_page(struct sg_page_iter *piter)
{
	return (nth_page(sg_page(piter->sg), piter->sg_pgoffset));
}

static inline dma_addr_t
sg_page_iter_dma_address(struct sg_dma_page_iter *it)
{
	return (sg_dma_address(it->base.sg) +
	    ((dma_addr_t)it->base.sg_pgoffset << PAGE_SHIFT));
}

#define	for_each_sg_page(sglist, piter, nents, pgoffset)		\
	for (__sg_page_iter_start((piter), (sglist), (nents), (pgoffset)); \
	    __sg_page_iter_next(piter);)
#define	for_each_sg_dma_page(sglist, dma_iter, dma_nents, pgoffset)	\
	for (__sg_page_iter_start(&(dma_iter)->base, (sglist), (dma_nents), \
	    (pgoffset)); __sg_page_iter_dma_next(dma_iter);)
#define	for_each_sgtable_page(sgt, piter, pgoffset)			\
	for_each_sg_page((sgt)->sgl, piter, (sgt)->orig_nents, pgoffset)
#define	for_each_sgtable_dma_page(sgt, dma_iter, pgoffset)		\
	for_each_sg_dma_page((sgt)->sgl, dma_iter, (sgt)->nents, pgoffset)

#define	sg_set_folio(sg, f, len, off)	sg_set_page((sg), &(f)->page, (len), (off))

#endif /* _LINUXKPI_LINUX_SCATTERLIST_H_ */
