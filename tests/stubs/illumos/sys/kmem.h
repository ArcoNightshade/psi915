/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for syntax checking only; see tests/stubs/illumos/README.md. */
#ifndef _STUB_SYS_KMEM_H
#define	_STUB_SYS_KMEM_H
#include <sys/types.h>
#define	KM_SLEEP	0x0000
#define	KM_NOSLEEP	0x0001
#define	KM_NORMALPRI	0x0008
typedef struct kmem_cache kmem_cache_t;
typedef struct vmem vmem_t;
extern void *kmem_alloc(size_t, int);
extern void *kmem_zalloc(size_t, int);
extern void kmem_free(void *, size_t);
extern kmem_cache_t *kmem_cache_create(char *, size_t, size_t,
    int (*)(void *, void *, int), void (*)(void *, void *),
    void (*)(void *), void *, vmem_t *, int);
extern void kmem_cache_destroy(kmem_cache_t *);
extern void *kmem_cache_alloc(kmem_cache_t *, int);
extern void kmem_cache_free(kmem_cache_t *, void *);
extern void kmem_cache_reap_soon(kmem_cache_t *);
#endif
