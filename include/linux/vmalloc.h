/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation from the documented behaviour of the Linux
 * vmalloc API. vmalloc memory comes from the same size-headered kmem
 * allocator as kvmalloc (<linux/slab.h>), so vfree() and kvfree() are
 * interchangeable. vmap()/vmalloc_to_page() need struct page (P2).
 */
#ifndef _LINUXKPI_LINUX_VMALLOC_H_
#define	_LINUXKPI_LINUX_VMALLOC_H_

#include <linuxkpi/platform.h>
#include <linux/slab.h>

static inline void *
vmalloc(unsigned long size)
{
	return (kvmalloc(size, GFP_KERNEL));
}

static inline void *
vzalloc(unsigned long size)
{
	return (kvzalloc(size, GFP_KERNEL));
}

static inline void *
__vmalloc(unsigned long size, gfp_t gfp)
{
	return (kvmalloc(size, gfp));
}

static inline void *
vmalloc_array(size_t n, size_t size)
{
	return (kvmalloc_array(n, size, GFP_KERNEL));
}

static inline void *
vcalloc(size_t n, size_t size)
{
	return (kvcalloc(n, size, GFP_KERNEL));
}

#define	vmalloc_node(size, node)	vmalloc(size)
#define	vzalloc_node(size, node)	vzalloc(size)
#define	vmalloc_32(size)		vmalloc(size)

static inline void
vfree(const void *p)
{
	kvfree(p);
}

/* No separate vmalloc arena: nothing is a "vmalloc address". */
static inline bool
is_vmalloc_addr(const void *p)
{
	(void) p;
	return (false);
}

/*
 * vmalloc memory comes from the slab allocator and has no struct page
 * unless it was allocated as pages: lkpi_virt_to_page() warns and
 * returns NULL for it.
 */
struct page;
struct page *lkpi_virt_to_page(const void *kva);
#define	vmalloc_to_page(a)	lkpi_virt_to_page(a)
struct notifier_block;
/* Never called: illumos has no vmalloc purge event (linux_intel_gfx.c). */
int register_vmap_purge_notifier(struct notifier_block *nb);
int unregister_vmap_purge_notifier(struct notifier_block *nb);
#define	is_vmalloc_addr(a)	((void)(a), false)

#endif /* _LINUXKPI_LINUX_VMALLOC_H_ */
