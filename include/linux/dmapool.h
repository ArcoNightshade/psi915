/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation of Linux DMA pools (src/linux_dmapool.c):
 * fixed-size blocks carved from dma_alloc_coherent() chunks, each block
 * aligned to `align` and never crossing a `boundary` (0 = none).
 */
#ifndef _LINUXKPI_LINUX_DMAPOOL_H_
#define	_LINUXKPI_LINUX_DMAPOOL_H_

#include <linux/types.h>
#include <linux/device.h>
#include <linux/gfp.h>

struct dma_pool;

struct dma_pool *dma_pool_create(const char *name, struct device *dev,
    size_t size, size_t align, size_t boundary);
void dma_pool_destroy(struct dma_pool *pool);
void *dma_pool_alloc(struct dma_pool *pool, gfp_t gfp, dma_addr_t *dma);
void dma_pool_free(struct dma_pool *pool, void *vaddr, dma_addr_t dma);
struct dma_pool *dmam_pool_create(const char *name, struct device *dev,
    size_t size, size_t align, size_t boundary);

static inline void *
dma_pool_zalloc(struct dma_pool *pool, gfp_t gfp, dma_addr_t *dma)
{
	return (dma_pool_alloc(pool, gfp | __GFP_ZERO, dma));
}

#endif /* _LINUXKPI_LINUX_DMAPOOL_H_ */
