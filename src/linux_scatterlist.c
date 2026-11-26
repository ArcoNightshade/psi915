/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * Scatterlist tables and page iterators (see <linux/scatterlist.h>).
 * Clean-room.
 */

#include <linux/kernel.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/highmem.h>

int
sg_nents(struct scatterlist *sg)
{
	int n = 0;

	for (; sg != NULL; sg = sg_next(sg))
		n++;
	return (n);
}

int
sg_alloc_table(struct sg_table *table, unsigned int nents, gfp_t gfp)
{
	memset(table, 0, sizeof (*table));
	if (nents == 0)
		return (-EINVAL);
	table->sgl = kvcalloc(nents, sizeof (struct scatterlist), gfp);
	if (table->sgl == NULL)
		return (-ENOMEM);
	sg_init_table(table->sgl, nents);
	table->nents = table->orig_nents = nents;
	return (0);
}

void
sg_free_table(struct sg_table *table)
{
	if (table->sgl != NULL)
		kvfree(table->sgl);
	table->sgl = NULL;
	table->nents = table->orig_nents = 0;
}

/*
 * One entry per run of physically contiguous pages, each at most
 * max_segment bytes (rounded down to whole pages).
 */
int
sg_alloc_table_from_pages_segment(struct sg_table *sgt, struct page **pages,
    unsigned int n_pages, unsigned int offset, unsigned long size,
    unsigned int max_segment, gfp_t gfp)
{
	unsigned int i, chunks = 0, seg_pages;
	struct scatterlist *sg;
	int r;

	max_segment &= PAGE_MASK;
	if (max_segment < PAGE_SIZE)
		max_segment = PAGE_SIZE;
	seg_pages = max_segment >> PAGE_SHIFT;
	if (n_pages == 0)
		return (-EINVAL);
	for (i = 0; i < n_pages; ) {
		unsigned int j = i + 1;

		while (j < n_pages && j - i < seg_pages &&
		    page_to_phys(pages[j]) ==
		    page_to_phys(pages[j - 1]) + PAGE_SIZE)
			j++;
		chunks++;
		i = j;
	}
	r = sg_alloc_table(sgt, chunks, gfp);
	if (r != 0)
		return (r);
	sg = sgt->sgl;
	for (i = 0; i < n_pages; ) {
		unsigned int j = i + 1, len;

		while (j < n_pages && j - i < seg_pages &&
		    page_to_phys(pages[j]) ==
		    page_to_phys(pages[j - 1]) + PAGE_SIZE)
			j++;
		len = (j - i) * (unsigned int)PAGE_SIZE - offset;
		if (len > size)
			len = (unsigned int)size;
		sg_set_page(sg, pages[i], len, offset);
		size -= len;
		offset = 0;
		sg = sg_next(sg);
		i = j;
	}
	return (0);
}

static size_t
lkpi_sg_copy(struct scatterlist *sgl, unsigned int nents, void *buf,
    size_t buflen, bool to_buffer)
{
	struct scatterlist *sg;
	size_t done = 0, n;
	unsigned int i;

	for_each_sg(sgl, sg, nents, i) {
		if (done >= buflen)
			break;
		n = sg->length < buflen - done ? sg->length : buflen - done;
		if (to_buffer)
			memcpy((char *)buf + done, sg_virt(sg), n);
		else
			memcpy(sg_virt(sg), (char *)buf + done, n);
		done += n;
	}
	return (done);
}

size_t
sg_copy_from_buffer(struct scatterlist *sgl, unsigned int nents,
    const void *buf, size_t buflen)
{
	return (lkpi_sg_copy(sgl, nents, (void *)(uintptr_t)buf, buflen,
	    false));
}

size_t
sg_copy_to_buffer(struct scatterlist *sgl, unsigned int nents, void *buf,
    size_t buflen)
{
	return (lkpi_sg_copy(sgl, nents, buf, buflen, true));
}

static int
lkpi_sg_page_count(struct scatterlist *sg)
{
	return ((int)(PAGE_ALIGN(sg->offset + sg->length) >> PAGE_SHIFT));
}

static int
lkpi_sg_dma_page_count(struct scatterlist *sg)
{
	return ((int)(PAGE_ALIGN(sg->offset + sg_dma_len(sg)) >> PAGE_SHIFT));
}

void
__sg_page_iter_start(struct sg_page_iter *piter, struct scatterlist *sgl,
    unsigned int nents, unsigned long pgoffset)
{
	piter->__pg_advance = 0;
	piter->__nents = nents;
	piter->sg = sgl;
	piter->sg_pgoffset = (unsigned int)pgoffset;
}

static bool
lkpi_sg_iter_next(struct sg_page_iter *piter, int (*count)(struct scatterlist *))
{
	if (piter->__nents == 0 || piter->sg == NULL)
		return (false);
	piter->sg_pgoffset += (unsigned int)piter->__pg_advance;
	piter->__pg_advance = 1;
	while ((int)piter->sg_pgoffset >= count(piter->sg)) {
		piter->sg_pgoffset -= (unsigned int)count(piter->sg);
		piter->sg = sg_next(piter->sg);
		if (--piter->__nents == 0 || piter->sg == NULL)
			return (false);
	}
	return (true);
}

bool
__sg_page_iter_next(struct sg_page_iter *piter)
{
	return (lkpi_sg_iter_next(piter, lkpi_sg_page_count));
}

bool
__sg_page_iter_dma_next(struct sg_dma_page_iter *dma_iter)
{
	return (lkpi_sg_iter_next(&dma_iter->base, lkpi_sg_dma_page_count));
}
