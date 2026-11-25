/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * Memory pools (see <linux/mempool.h>). Clean-room.
 */

#include <linux/kernel.h>
#include <linux/mempool.h>
#include <linux/slab.h>
#include <linux/bug.h>

void *
mempool_alloc_slab(gfp_t gfp, void *pool_data)
{
	return (kmem_cache_alloc(pool_data, gfp));
}

void
mempool_free_slab(void *element, void *pool_data)
{
	kmem_cache_free(pool_data, element);
}

void *
mempool_kmalloc(gfp_t gfp, void *pool_data)
{
	return (kmalloc((size_t)(uintptr_t)pool_data, gfp));
}

void
mempool_kfree(void *element, void *pool_data)
{
	(void) pool_data;
	kfree(element);
}

void
mempool_exit(mempool_t *pool)
{
	if (pool->elements == NULL)
		return;
	while (pool->curr_nr > 0)
		pool->free(pool->elements[--pool->curr_nr], pool->pool_data);
	kfree(pool->elements);
	pool->elements = NULL;
	lkpi_plat_cv_destroy(&pool->wait);
	lkpi_plat_mutex_destroy(&pool->lock);
}

int
mempool_init(mempool_t *pool, int min_nr, mempool_alloc_t *alloc_fn,
    mempool_free_t *free_fn, void *pool_data)
{
	void *e;

	memset(pool, 0, sizeof (*pool));
	if (min_nr < 1)
		min_nr = 1;
	lkpi_plat_mutex_init(&pool->lock, NULL);
	lkpi_plat_cv_init(&pool->wait);
	pool->min_nr = min_nr;
	pool->alloc = alloc_fn;
	pool->free = free_fn;
	pool->pool_data = pool_data;
	pool->elements = kmalloc_array((size_t)min_nr, sizeof (void *),
	    GFP_KERNEL);
	if (pool->elements == NULL)
		goto fail;
	while (pool->curr_nr < min_nr) {
		e = alloc_fn(GFP_KERNEL, pool_data);
		if (e == NULL) {
			mempool_exit(pool);
			return (-ENOMEM);
		}
		pool->elements[pool->curr_nr++] = e;
	}
	return (0);
fail:
	lkpi_plat_cv_destroy(&pool->wait);
	lkpi_plat_mutex_destroy(&pool->lock);
	return (-ENOMEM);
}

mempool_t *
mempool_create(int min_nr, mempool_alloc_t *alloc_fn, mempool_free_t *free_fn,
    void *pool_data)
{
	mempool_t *pool = kzalloc(sizeof (*pool), GFP_KERNEL);

	if (pool == NULL)
		return (NULL);
	if (mempool_init(pool, min_nr, alloc_fn, free_fn, pool_data) != 0) {
		kfree(pool);
		return (NULL);
	}
	return (pool);
}

void
mempool_destroy(mempool_t *pool)
{
	if (pool == NULL)
		return;
	mempool_exit(pool);
	kfree(pool);
}

void *
mempool_alloc(mempool_t *pool, gfp_t gfp)
{
	gfp_t try = (gfp & ~(__GFP_DIRECT_RECLAIM | __GFP_IO | __GFP_FS)) |
	    __GFP_NOWARN | __GFP_NORETRY;
	void *e;

	for (;;) {
		e = pool->alloc(try, pool->pool_data);
		if (e != NULL)
			return (e);
		lkpi_plat_mutex_enter(&pool->lock);
		if (pool->curr_nr > 0) {
			e = pool->elements[--pool->curr_nr];
			lkpi_plat_mutex_exit(&pool->lock);
			return (e);
		}
		if (!gfpflags_allow_blocking(gfp)) {
			lkpi_plat_mutex_exit(&pool->lock);
			return (NULL);
		}
		/* Wait for a free (or retry the allocator after 100 ms). */
		(void) lkpi_plat_cv_timedwait_ns(&pool->wait, &pool->lock,
		    lkpi_plat_nsec() + 100000000ULL, false);
		lkpi_plat_mutex_exit(&pool->lock);
	}
}

void
mempool_free(void *element, mempool_t *pool)
{
	if (element == NULL)
		return;
	lkpi_plat_mutex_enter(&pool->lock);
	if (pool->curr_nr < pool->min_nr) {
		pool->elements[pool->curr_nr++] = element;
		lkpi_plat_cv_signal(&pool->wait);
		lkpi_plat_mutex_exit(&pool->lock);
		return;
	}
	lkpi_plat_mutex_exit(&pool->lock);
	pool->free(element, pool->pool_data);
}

int
mempool_resize(mempool_t *pool, int new_min_nr)
{
	void **ne, *e;

	if (new_min_nr < 1)
		return (-EINVAL);
	ne = kmalloc_array((size_t)new_min_nr, sizeof (void *), GFP_KERNEL);
	if (ne == NULL)
		return (-ENOMEM);
	lkpi_plat_mutex_enter(&pool->lock);
	while (pool->curr_nr > new_min_nr) {
		e = pool->elements[--pool->curr_nr];
		lkpi_plat_mutex_exit(&pool->lock);
		pool->free(e, pool->pool_data);
		lkpi_plat_mutex_enter(&pool->lock);
	}
	memcpy(ne, pool->elements, (size_t)pool->curr_nr * sizeof (void *));
	kfree(pool->elements);
	pool->elements = ne;
	pool->min_nr = new_min_nr;
	lkpi_plat_mutex_exit(&pool->lock);
	while (pool->curr_nr < new_min_nr) {
		e = pool->alloc(GFP_KERNEL, pool->pool_data);
		if (e == NULL)
			break;
		mempool_free(e, pool);
	}
	return (0);
}
