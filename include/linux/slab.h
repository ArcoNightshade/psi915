/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation from the documented behaviour of the Linux
 * slab API; function set cross-checked against FreeBSD's LinuxKPI slab.h
 * (BSD-2-Clause).
 *
 * kmalloc and friends (src/linux_slab.c) put a 16-byte header holding the
 * size in front of each block, because kmem_free(9F) needs the size and
 * Linux kfree() does not pass it. Consequences:
 *   - blocks are 16-byte aligned; Linux also guarantees natural alignment
 *     for power-of-two sizes, which is NOT provided (use a kmem_cache with
 *     an explicit align, or the page allocator, for that);
 *   - kfree(), kvfree() and vfree() are interchangeable;
 *   - ksize() returns the requested size.
 * kmalloc() fails above KMALLOC_MAX_SIZE (4 MiB) like Linux; kvmalloc and
 * vmalloc accept up to LKPI_KVMALLOC_MAX. Memory comes from kmem_alloc(9F)
 * and is not physically contiguous beyond a page: device DMA must use the
 * DMA API (P2). See <linux/gfp.h> for how gfp flags map to KM_* flags.
 *
 * struct kmem_cache is an opaque handle for a kmem_cache_create(9F) cache
 * (constructors keep illumos/Linux constructed-object semantics). The
 * kmem_cache_* names collide with illumos <sys/kmem.h>, so they are macros
 * over lkpi_kmem_cache_*; illumos headers must be included first.
 */
#ifndef _LINUXKPI_LINUX_SLAB_H_
#define	_LINUXKPI_LINUX_SLAB_H_

#include <linuxkpi/platform.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/gfp.h>
#include <linux/overflow.h>
#include <linux/atomic.h>

#ifndef NUMA_NO_NODE
#define	NUMA_NO_NODE		(-1)
#endif

#define	ZERO_SIZE_PTR		((void *)16)
#define	ZERO_OR_NULL_PTR(x)						\
	((unsigned long)(x) <= (unsigned long)ZERO_SIZE_PTR)

#define	ARCH_KMALLOC_MINALIGN	16
#define	ARCH_SLAB_MINALIGN	8
#define	KMALLOC_MAX_SIZE	(1UL << 22)
#define	KMALLOC_MAX_CACHE_SIZE	KMALLOC_MAX_SIZE
#define	LKPI_KVMALLOC_MAX	(1UL << 34)

/* Live kmalloc-family allocations (for leak checks in tests). */
extern atomic_long_t lkpi_kmalloc_live;

void *kmalloc(size_t size, gfp_t gfp) __malloc;
void kfree(const void *p);
void *krealloc(const void *p, size_t new_size, gfp_t gfp) __must_check;
size_t ksize(const void *p);
void *kvmalloc_node(size_t size, gfp_t gfp, int node) __malloc;
void kvfree(const void *p);
void *kvrealloc(const void *p, size_t size, gfp_t gfp) __must_check;
void kfree_sensitive(const void *p);

static inline void *
kzalloc(size_t size, gfp_t gfp)
{
	return (kmalloc(size, gfp | __GFP_ZERO));
}

static inline void *
kmalloc_array(size_t n, size_t size, gfp_t gfp)
{
	return (kmalloc(size_mul(n, size), gfp));
}

static inline void *
kcalloc(size_t n, size_t size, gfp_t gfp)
{
	return (kmalloc_array(n, size, gfp | __GFP_ZERO));
}

static inline void *
krealloc_array(void *p, size_t n, size_t size, gfp_t gfp)
{
	return (krealloc(p, size_mul(n, size), gfp));
}

#define	kmalloc_node(size, gfp, node)		kmalloc(size, gfp)
#define	kzalloc_node(size, gfp, node)		kzalloc(size, gfp)
#define	kcalloc_node(n, size, gfp, node)	kcalloc(n, size, gfp)
#define	kmalloc_array_node(n, size, gfp, node)	kmalloc_array(n, size, gfp)
#define	kmalloc_track_caller(size, gfp)		kmalloc(size, gfp)

static inline size_t
kmalloc_size_roundup(size_t size)
{
	return (size);
}

static inline void *
kvmalloc(size_t size, gfp_t gfp)
{
	return (kvmalloc_node(size, gfp, NUMA_NO_NODE));
}

static inline void *
kvzalloc(size_t size, gfp_t gfp)
{
	return (kvmalloc(size, gfp | __GFP_ZERO));
}

#define	kvzalloc_node(size, gfp, node)	kvmalloc_node(size, (gfp) | __GFP_ZERO, node)

static inline void *
kvmalloc_array(size_t n, size_t size, gfp_t gfp)
{
	return (kvmalloc(size_mul(n, size), gfp));
}

static inline void *
kvcalloc(size_t n, size_t size, gfp_t gfp)
{
	return (kvmalloc_array(n, size, gfp | __GFP_ZERO));
}

static inline void
kvfree_sensitive(const void *p, size_t len)
{
	(void) len;
	kfree_sensitive(p);
}

/* Object caches. */
typedef unsigned int slab_flags_t;

#define	SLAB_CONSISTENCY_CHECKS	((slab_flags_t)0x00000100)
#define	SLAB_RED_ZONE		((slab_flags_t)0x00000400)
#define	SLAB_POISON		((slab_flags_t)0x00000800)
#define	SLAB_KMALLOC		((slab_flags_t)0x00001000)
#define	SLAB_HWCACHE_ALIGN	((slab_flags_t)0x00002000)
#define	SLAB_CACHE_DMA		((slab_flags_t)0x00004000)
#define	SLAB_CACHE_DMA32	((slab_flags_t)0x00008000)
#define	SLAB_STORE_USER		((slab_flags_t)0x00010000)
#define	SLAB_PANIC		((slab_flags_t)0x00040000)
#define	SLAB_TYPESAFE_BY_RCU	((slab_flags_t)0x00080000)
#define	SLAB_TRACE		((slab_flags_t)0x00200000)
#define	SLAB_NOLEAKTRACE	((slab_flags_t)0x00800000)
#define	SLAB_NO_MERGE		((slab_flags_t)0x01000000)
#define	SLAB_ACCOUNT		((slab_flags_t)0x04000000)
#define	SLAB_RECLAIM_ACCOUNT	((slab_flags_t)0x00020000)
#define	SLAB_TEMPORARY		SLAB_RECLAIM_ACCOUNT
#define	SLAB_MEM_SPREAD		((slab_flags_t)0)

#define	L1_CACHE_BYTES		64
#define	SMP_CACHE_BYTES		L1_CACHE_BYTES

struct kmem_cache;

struct kmem_cache *lkpi_kmem_cache_create(const char *name, unsigned int size,
    unsigned int align, slab_flags_t flags, void (*ctor)(void *));
void *lkpi_kmem_cache_alloc(struct kmem_cache *c, gfp_t gfp) __malloc;
void lkpi_kmem_cache_free(struct kmem_cache *c, void *obj);
void lkpi_kmem_cache_destroy(struct kmem_cache *c);
int lkpi_kmem_cache_shrink(struct kmem_cache *c);
unsigned int lkpi_kmem_cache_size(struct kmem_cache *c);

#define	kmem_cache_create(name, size, align, flags, ctor)		\
	lkpi_kmem_cache_create(name, size, align, flags, ctor)
#define	kmem_cache_create_usercopy(name, size, align, flags, uoff, usize, ctor) \
	lkpi_kmem_cache_create(name, size, align, flags, ctor)
#define	kmem_cache_alloc(c, gfp)	lkpi_kmem_cache_alloc(c, gfp)
#define	kmem_cache_alloc_node(c, gfp, node) lkpi_kmem_cache_alloc(c, gfp)
#define	kmem_cache_zalloc(c, gfp)					\
	lkpi_kmem_cache_alloc(c, (gfp) | __GFP_ZERO)
#define	kmem_cache_free(c, obj)		lkpi_kmem_cache_free(c, obj)
#define	kmem_cache_destroy(c)		lkpi_kmem_cache_destroy(c)
#define	kmem_cache_shrink(c)		lkpi_kmem_cache_shrink(c)
#define	kmem_cache_size(c)		lkpi_kmem_cache_size(c)

#define	KMEM_CACHE(s, flags)						\
	lkpi_kmem_cache_create(#s, sizeof(struct s), __alignof__(struct s), \
	    (flags), NULL)
#define	KMEM_CACHE_USERCOPY(s, flags, field)	KMEM_CACHE(s, flags)

#endif /* _LINUXKPI_LINUX_SLAB_H_ */
