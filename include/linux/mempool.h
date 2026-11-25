/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation of Linux memory pools (src/linux_mempool.c)
 * from their documented behaviour: a reserve of min_nr preallocated
 * elements backs the allocator. mempool_alloc() tries the allocator
 * first (without sleeping), then the reserve; if both are empty and gfp
 * allows sleeping, it waits for mempool_free() to return an element, so
 * it never fails for sleeping callers. mempool_free() refills the reserve
 * before returning memory to the allocator.
 */
#ifndef _LINUXKPI_LINUX_MEMPOOL_H_
#define	_LINUXKPI_LINUX_MEMPOOL_H_

#include <linuxkpi/platform.h>
#include <linux/types.h>
#include <linux/gfp.h>
#include <linux/slab.h>

typedef void *(mempool_alloc_t)(gfp_t gfp, void *pool_data);
typedef void (mempool_free_t)(void *element, void *pool_data);

typedef struct mempool {
	lkpi_plat_mutex_t	lock;
	lkpi_plat_cv_t		wait;
	int			min_nr;
	int			curr_nr;
	void			**elements;
	void			*pool_data;
	mempool_alloc_t		*alloc;
	mempool_free_t		*free;
} mempool_t;

int mempool_init(mempool_t *pool, int min_nr, mempool_alloc_t *alloc_fn,
    mempool_free_t *free_fn, void *pool_data);
void mempool_exit(mempool_t *pool);
mempool_t *mempool_create(int min_nr, mempool_alloc_t *alloc_fn,
    mempool_free_t *free_fn, void *pool_data);
void mempool_destroy(mempool_t *pool);
void *mempool_alloc(mempool_t *pool, gfp_t gfp) __malloc;
void mempool_free(void *element, mempool_t *pool);
int mempool_resize(mempool_t *pool, int new_min_nr);

static inline bool
mempool_initialized(mempool_t *pool)
{
	return (pool->elements != NULL);
}

void *mempool_alloc_slab(gfp_t gfp, void *pool_data);
void mempool_free_slab(void *element, void *pool_data);
void *mempool_kmalloc(gfp_t gfp, void *pool_data);
void mempool_kfree(void *element, void *pool_data);

#define	mempool_create_slab_pool(n, cache)				\
	mempool_create((n), mempool_alloc_slab, mempool_free_slab, (cache))
#define	mempool_init_slab_pool(p, n, cache)				\
	mempool_init((p), (n), mempool_alloc_slab, mempool_free_slab, (cache))
#define	mempool_create_kmalloc_pool(n, size)				\
	mempool_create((n), mempool_kmalloc, mempool_kfree,		\
	    (void *)(uintptr_t)(size))
#define	mempool_init_kmalloc_pool(p, n, size)				\
	mempool_init((p), (n), mempool_kmalloc, mempool_kfree,		\
	    (void *)(uintptr_t)(size))

#endif /* _LINUXKPI_LINUX_MEMPOOL_H_ */
