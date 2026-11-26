/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * DMA pools (see <linux/dmapool.h>). Clean-room. Chunks of at least one
 * page come from dma_alloc_coherent(); each holds as many blocks as fit
 * at multiples of the rounded-up block size, skipping blocks that would
 * cross a boundary. Free blocks are chained through their first word.
 */

#include <linux/kernel.h>
#include <linux/dmapool.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/string.h>
#include <linux/bug.h>

struct lkpi_dma_chunk {
	struct list_head	entry;
	void			*vaddr;
	dma_addr_t		dma;
	unsigned int		in_use;
};

struct dma_pool {
	struct device		*dev;
	char			name[32];
	size_t			size;		/* rounded block size */
	size_t			boundary;
	size_t			chunk_size;
	struct list_head	chunks;
	void			*free;		/* free block list */
	struct mutex		lock;
};

struct dma_pool *
dma_pool_create(const char *name, struct device *dev, size_t size,
    size_t align, size_t boundary)
{
	struct dma_pool *pool;

	if (align == 0)
		align = 1;
	if ((align & (align - 1)) != 0 || size == 0)
		return (NULL);
	if (size < sizeof (void *))
		size = sizeof (void *);
	size = ALIGN(size, align < sizeof (void *) ? sizeof (void *) : align);
	if (boundary != 0 && ((boundary & (boundary - 1)) != 0 ||
	    boundary < size))
		return (NULL);
	pool = kzalloc(sizeof (*pool), GFP_KERNEL);
	if (pool == NULL)
		return (NULL);
	(void) strscpy(pool->name, name, sizeof (pool->name));
	pool->dev = dev;
	pool->size = size;
	pool->boundary = boundary;
	pool->chunk_size = size > PAGE_SIZE ? ALIGN(size, PAGE_SIZE) : PAGE_SIZE;
	if (boundary != 0 && pool->chunk_size > boundary)
		pool->chunk_size = boundary;
	INIT_LIST_HEAD(&pool->chunks);
	mutex_init(&pool->lock);
	return (pool);
}

void
dma_pool_destroy(struct dma_pool *pool)
{
	struct lkpi_dma_chunk *c, *tmp;

	if (pool == NULL)
		return;
	list_for_each_entry_safe(c, tmp, &pool->chunks, entry) {
		WARN(c->in_use != 0, "linuxkpi: dma_pool %s destroyed with "
		    "%u blocks in use\n", pool->name, c->in_use);
		dma_free_coherent(pool->dev, pool->chunk_size, c->vaddr, c->dma);
		list_del(&c->entry);
		kfree(c);
	}
	mutex_destroy(&pool->lock);
	kfree(pool);
}

/* Caller holds the lock. */
static int
lkpi_dma_pool_grow(struct dma_pool *pool, gfp_t gfp)
{
	struct lkpi_dma_chunk *c;
	size_t off;

	c = kzalloc(sizeof (*c), gfp);
	if (c == NULL)
		return (-ENOMEM);
	c->vaddr = dma_alloc_coherent(pool->dev, pool->chunk_size, &c->dma,
	    gfp);
	if (c->vaddr == NULL) {
		kfree(c);
		return (-ENOMEM);
	}
	for (off = 0; off + pool->size <= pool->chunk_size;
	    off += pool->size) {
		if (pool->boundary != 0 &&
		    (off / pool->boundary) !=
		    ((off + pool->size - 1) / pool->boundary)) {
			off = ALIGN(off, pool->boundary) - pool->size;
			continue;
		}
		*(void **)((char *)c->vaddr + off) = pool->free;
		pool->free = (char *)c->vaddr + off;
	}
	list_add(&c->entry, &pool->chunks);
	return (0);
}

static struct lkpi_dma_chunk *
lkpi_dma_pool_chunk(struct dma_pool *pool, void *vaddr)
{
	struct lkpi_dma_chunk *c;

	list_for_each_entry(c, &pool->chunks, entry)
		if ((char *)vaddr >= (char *)c->vaddr &&
		    (char *)vaddr < (char *)c->vaddr + pool->chunk_size)
			return (c);
	return (NULL);
}

void *
dma_pool_alloc(struct dma_pool *pool, gfp_t gfp, dma_addr_t *dma)
{
	struct lkpi_dma_chunk *c;
	void *b;

	mutex_lock(&pool->lock);
	if (pool->free == NULL && lkpi_dma_pool_grow(pool, gfp) != 0) {
		mutex_unlock(&pool->lock);
		return (NULL);
	}
	b = pool->free;
	pool->free = *(void **)b;
	c = lkpi_dma_pool_chunk(pool, b);
	c->in_use++;
	*dma = c->dma + (dma_addr_t)((char *)b - (char *)c->vaddr);
	mutex_unlock(&pool->lock);
	if (gfp & __GFP_ZERO)
		memset(b, 0, pool->size);
	return (b);
}

void
dma_pool_free(struct dma_pool *pool, void *vaddr, dma_addr_t dma)
{
	struct lkpi_dma_chunk *c;

	mutex_lock(&pool->lock);
	c = lkpi_dma_pool_chunk(pool, vaddr);
	if (c == NULL || c->dma + (dma_addr_t)((char *)vaddr -
	    (char *)c->vaddr) != dma) {
		mutex_unlock(&pool->lock);
		WARN(1, "linuxkpi: dma_pool_free(%s) of bad block %p\n",
		    pool->name, vaddr);
		return;
	}
	c->in_use--;
	*(void **)vaddr = pool->free;
	pool->free = vaddr;
	mutex_unlock(&pool->lock);
}

static void
lkpi_dmam_pool_release(struct device *dev, void *res)
{
	(void) dev;
	dma_pool_destroy(*(struct dma_pool **)res);
}

struct dma_pool *
dmam_pool_create(const char *name, struct device *dev, size_t size,
    size_t align, size_t boundary)
{
	struct dma_pool **p;

	p = devres_alloc(lkpi_dmam_pool_release, sizeof (*p), GFP_KERNEL);
	if (p == NULL)
		return (NULL);
	*p = dma_pool_create(name, dev, size, align, boundary);
	if (*p == NULL) {
		devres_free(p);
		return (NULL);
	}
	devres_add(dev, *p == NULL ? NULL : p);
	return (*p);
}
