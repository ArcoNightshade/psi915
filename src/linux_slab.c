/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * kmalloc/kvmalloc with a size header, kmem_cache wrappers, kstrdup and
 * friends (see <linux/slab.h>). Clean-room.
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/string.h>
#include <linux/bug.h>
#include <linux/rcupdate.h>

#define	LKPI_KMALLOC_MAGIC	0x6b6d6c63U	/* "kmlc" */
#define	LKPI_KMALLOC_FREED	0x66726565U	/* "free" */

/*
 * 16 bytes, so the payload keeps at least 16-byte alignment. Power-of-two
 * sizes above 16 bytes are naturally aligned (up to PAGE_SIZE), as Linux
 * guarantees since 5.4 (docs/12 M4): such allocations get alignment bytes
 * of slack and the header sits right before the aligned payload; offset is
 * the distance from the raw allocation to the payload.
 */
struct lkpi_kmalloc_hdr {
	size_t		size;
	uint32_t	magic;
	uint32_t	offset;
};

static size_t
lkpi_kmalloc_align(size_t size)
{
	if (size <= 16 || (size & (size - 1)) != 0)
		return (0);
	return (size < PAGE_SIZE ? size : PAGE_SIZE);
}

/* Bytes allocated from the platform for a payload of size bytes. */
static size_t
lkpi_kmalloc_total(size_t size)
{
	size_t a = lkpi_kmalloc_align(size);

	return (size + (a != 0 ? a : sizeof (struct lkpi_kmalloc_hdr)));
}

atomic_long_t lkpi_kmalloc_live;

size_t
lkpi_kvmalloc_max(void)
{
	size_t lim = (size_t)1 << 34;
	size_t q = (size_t)lkpi_plat_physpages() / 4 * 4096;

	return (q != 0 && q < lim ? q : lim);
}

static void *
lkpi_alloc_hdr(size_t size, gfp_t gfp, size_t limit)
{
	struct lkpi_kmalloc_hdr *h;
	size_t total, align;
	char *raw, *payload;

	if (size == 0)
		return (ZERO_SIZE_PTR);
	if (size > limit ||
	    __builtin_add_overflow(size, PAGE_SIZE, &total)) {
		if (!(gfp & __GFP_NOWARN))
			pr_warn("linuxkpi: allocation of %zu bytes refused "
			    "(limit %zu)\n", size, limit);
		return (NULL);
	}
	total = lkpi_kmalloc_total(size);
	align = lkpi_kmalloc_align(size);
	if (gfp & __GFP_ZERO)
		raw = lkpi_plat_zalloc(total, lkpi_gfp_to_plat_size(gfp, total));
	else
		raw = lkpi_plat_alloc(total, lkpi_gfp_to_plat_size(gfp, total));
	if (raw == NULL)
		return (NULL);
	if (align != 0)
		payload = (char *)(((uintptr_t)raw + sizeof (*h) + align - 1) &
		    ~(uintptr_t)(align - 1));
	else
		payload = raw + sizeof (*h);
	h = (struct lkpi_kmalloc_hdr *)(void *)payload - 1;
	h->size = size;
	h->magic = LKPI_KMALLOC_MAGIC;
	h->offset = (uint32_t)(payload - raw);
	atomic_long_inc(&lkpi_kmalloc_live);
	return (payload);
}

static struct lkpi_kmalloc_hdr *
lkpi_hdr(const void *p)
{
	struct lkpi_kmalloc_hdr *h;

	h = (struct lkpi_kmalloc_hdr *)(uintptr_t)p - 1;
	if (unlikely(h->magic != LKPI_KMALLOC_MAGIC)) {
		WARN(1, "linuxkpi: %p was not allocated by kmalloc (%s)\n", p,
		    h->magic == LKPI_KMALLOC_FREED ? "double free" :
		    "bad magic");
		return (NULL);
	}
	return (h);
}

void *
kmalloc(size_t size, gfp_t gfp)
{
	return (lkpi_alloc_hdr(size, gfp, KMALLOC_MAX_SIZE));
}

void *
kvmalloc_node(size_t size, gfp_t gfp, int node)
{
	(void) node;
	return (lkpi_alloc_hdr(size, gfp, LKPI_KVMALLOC_MAX));
}

void
kfree(const void *p)
{
	struct lkpi_kmalloc_hdr *h;

	if (ZERO_OR_NULL_PTR(p))
		return;
	h = lkpi_hdr(p);
	if (h == NULL)
		return;		/* leak rather than corrupt the allocator */
	h->magic = LKPI_KMALLOC_FREED;
	atomic_long_dec(&lkpi_kmalloc_live);
	lkpi_plat_free((char *)(uintptr_t)p - h->offset,
	    lkpi_kmalloc_total(h->size));
}

void
kvfree(const void *p)
{
	kfree(p);
}

void
kfree_sensitive(const void *p)
{
	struct lkpi_kmalloc_hdr *h;

	if (ZERO_OR_NULL_PTR(p))
		return;
	h = lkpi_hdr(p);
	if (h != NULL)
		memzero_explicit((void *)(uintptr_t)p, h->size);
	kfree(p);
}

size_t
ksize(const void *p)
{
	struct lkpi_kmalloc_hdr *h;

	if (ZERO_OR_NULL_PTR(p))
		return (0);
	h = lkpi_hdr(p);
	return (h != NULL ? h->size : 0);
}

static void *
lkpi_realloc(const void *p, size_t new_size, gfp_t gfp, size_t limit)
{
	size_t old_size;
	void *n;

	if (ZERO_OR_NULL_PTR(p))
		return (lkpi_alloc_hdr(new_size, gfp, limit));
	if (new_size == 0) {
		kfree(p);
		return (ZERO_SIZE_PTR);
	}
	old_size = ksize(p);
	if (new_size <= old_size)
		return ((void *)(uintptr_t)p);
	n = lkpi_alloc_hdr(new_size, gfp & ~__GFP_ZERO, limit);
	if (n == NULL)
		return (NULL);	/* p is left untouched, as in Linux */
	memcpy(n, p, old_size);
	if (gfp & __GFP_ZERO)
		memset((char *)n + old_size, 0, new_size - old_size);
	kfree(p);
	return (n);
}

void *
krealloc(const void *p, size_t new_size, gfp_t gfp)
{
	return (lkpi_realloc(p, new_size, gfp, KMALLOC_MAX_SIZE));
}

void *
kvrealloc(const void *p, size_t size, gfp_t gfp)
{
	return (lkpi_realloc(p, size, gfp, LKPI_KVMALLOC_MAX));
}

void *
kmemdup(const void *src, size_t len, gfp_t gfp)
{
	void *p = kmalloc(len, gfp);

	if (p != NULL && len != 0)
		memcpy(p, src, len);
	return (p);
}

void *
kmemdup_array(const void *src, size_t count, size_t elsize, gfp_t gfp)
{
	return (kmemdup(src, size_mul(count, elsize), gfp));
}

char *
kmemdup_nul(const char *s, size_t len, gfp_t gfp)
{
	char *p;

	if (s == NULL)
		return (NULL);
	p = kmalloc(size_add(len, 1), gfp);
	if (p != NULL) {
		memcpy(p, s, len);
		p[len] = '\0';
	}
	return (p);
}

char *
kstrdup(const char *s, gfp_t gfp)
{
	if (s == NULL)
		return (NULL);
	return (kmemdup_nul(s, strlen(s), gfp));
}

char *
kstrndup(const char *s, size_t max, gfp_t gfp)
{
	if (s == NULL)
		return (NULL);
	return (kmemdup_nul(s, strnlen(s, max), gfp));
}

/* No .rodata detection: always duplicate; kfree_const() always frees. */
const char *
kstrdup_const(const char *s, gfp_t gfp)
{
	return (kstrdup(s, gfp));
}

void
kfree_const(const void *p)
{
	kfree(p);
}

/*
 * Object caches. The Linux handle points at this wrapper; the struct
 * kmem_cache tag itself is never defined.
 *
 * SLAB_TYPESAFE_BY_RCU: a freed object may be reused for a new object of
 * the same cache at once in Linux, but its memory is not returned to the
 * system until a grace period has passed, so RCU readers can still look at
 * it (and must re-validate it). Here kmem_cache_free() of such an object
 * defers the real free with call_rcu(), which is stricter (no reuse within
 * the grace period either). The rcu_head lives in a trailer after the
 * object, at rcu_off, so the object's own bytes are left untouched.
 */
struct lkpi_kmem_cache {
	lkpi_plat_kcache_t	*kc;
	void			(*ctor)(void *);
	unsigned int		size;
	unsigned int		rcu_off;
	slab_flags_t		flags;
	atomic_long_t		live;
	char			name[32];
};

struct lkpi_kmem_cache_rcu {
	struct rcu_head		rh;
	struct lkpi_kmem_cache	*cache;
};

#define	LKPI_KC(c)	((struct lkpi_kmem_cache *)(void *)(c))

static int
lkpi_kmem_cache_ctor(void *buf, void *priv, int kmflags)
{
	struct lkpi_kmem_cache *c = priv;

	(void) kmflags;
	c->ctor(buf);
	return (0);
}

struct kmem_cache *
lkpi_kmem_cache_create(const char *name, unsigned int size,
    unsigned int align, slab_flags_t flags, void (*ctor)(void *))
{
	struct lkpi_kmem_cache *c;

	c = lkpi_plat_zalloc(sizeof (*c), LKPI_PLAT_SLEEP);
	if (c == NULL)
		goto fail;
	(void) strscpy(c->name, name != NULL ? name : "linuxkpi",
	    sizeof (c->name));
	c->ctor = ctor;
	c->size = size;
	c->flags = flags;
	if ((flags & SLAB_HWCACHE_ALIGN) && align < L1_CACHE_BYTES)
		align = L1_CACHE_BYTES;
	if (flags & SLAB_TYPESAFE_BY_RCU) {
		c->rcu_off = ALIGN(size,
		    (unsigned int)__alignof__(struct lkpi_kmem_cache_rcu));
		size = c->rcu_off + sizeof (struct lkpi_kmem_cache_rcu);
	}
	c->kc = lkpi_plat_kcache_create(c->name, size, align,
	    ctor != NULL ? lkpi_kmem_cache_ctor : NULL, c);
	if (c->kc == NULL) {
		lkpi_plat_free(c, sizeof (*c));
		goto fail;
	}
	return ((struct kmem_cache *)(void *)c);
fail:
	if (flags & SLAB_PANIC)
		panic("linuxkpi: kmem_cache_create(%s) failed",
		    name != NULL ? name : "?");
	return (NULL);
}

void *
lkpi_kmem_cache_alloc(struct kmem_cache *cache, gfp_t gfp)
{
	struct lkpi_kmem_cache *c = LKPI_KC(cache);
	void *obj;

	obj = lkpi_plat_kcache_alloc(c->kc, lkpi_gfp_to_plat(gfp));
	if (obj == NULL)
		return (NULL);
	if (gfp & __GFP_ZERO)
		memset(obj, 0, c->size);
	atomic_long_inc(&c->live);
	return (obj);
}

static void
lkpi_kmem_cache_rcu_free(struct rcu_head *rh)
{
	struct lkpi_kmem_cache_rcu *t =
	    container_of(rh, struct lkpi_kmem_cache_rcu, rh);
	struct lkpi_kmem_cache *c = t->cache;

	lkpi_plat_kcache_free(c->kc, (char *)t - c->rcu_off);
}

void
lkpi_kmem_cache_free(struct kmem_cache *cache, void *obj)
{
	struct lkpi_kmem_cache *c = LKPI_KC(cache);
	struct lkpi_kmem_cache_rcu *t;

	if (obj == NULL)
		return;
	atomic_long_dec(&c->live);
	if (c->flags & SLAB_TYPESAFE_BY_RCU) {
		t = (struct lkpi_kmem_cache_rcu *)((char *)obj + c->rcu_off);
		t->cache = c;
		call_rcu(&t->rh, lkpi_kmem_cache_rcu_free);
		return;
	}
	lkpi_plat_kcache_free(c->kc, obj);
}

void
lkpi_kmem_cache_destroy(struct kmem_cache *cache)
{
	struct lkpi_kmem_cache *c = LKPI_KC(cache);

	if (c == NULL)
		return;
	/* Deferred frees of a TYPESAFE_BY_RCU cache must land first. */
	if (c->flags & SLAB_TYPESAFE_BY_RCU)
		rcu_barrier();
	/*
	 * A cache with live objects is reported and left alone (Linux leaks
	 * it); destroying it would leave the objects dangling.
	 */
	if (WARN(atomic_long_read(&c->live) != 0,
	    "linuxkpi: kmem_cache_destroy(%s): %ld objects still allocated\n",
	    c->name, atomic_long_read(&c->live)))
		return;
	lkpi_plat_kcache_destroy(c->kc);
	lkpi_plat_free(c, sizeof (*c));
}

int
lkpi_kmem_cache_shrink(struct kmem_cache *cache)
{
	lkpi_plat_kcache_reap(LKPI_KC(cache)->kc);
	return (0);
}

unsigned int
lkpi_kmem_cache_size(struct kmem_cache *cache)
{
	return (LKPI_KC(cache)->size);
}
