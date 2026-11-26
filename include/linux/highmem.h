/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. amd64 has no highmem: every kmap variant returns the
 * page's permanent kernel address and every kunmap is a no-op.
 */
#ifndef _LINUXKPI_LINUX_HIGHMEM_H_
#define	_LINUXKPI_LINUX_HIGHMEM_H_

#include <linux/mm.h>
#include <linux/string.h>
#include <linux/uaccess.h>

static inline void *kmap(struct page *p) { return (page_address(p)); }
static inline void *kmap_atomic(struct page *p) { return (page_address(p)); }
static inline void *kmap_local_page(struct page *p) { return (page_address(p)); }
/* Folios are single pages here. */
#define	offset_in_folio(f, p)	((size_t)((unsigned long)(p) & (PAGE_SIZE - 1)))
#define	memcpy_to_folio(f, off, src, len)				\
	memcpy((char *)page_address(&(f)->page) + (off), (src), (len))
#define	memcpy_from_folio(dst, f, off, len)				\
	memcpy((dst), (char *)page_address(&(f)->page) + (off), (len))
#define	kmap_local_folio(f, off)	((char *)page_address(&(f)->page) + (off))
#define	kmap_atomic_prot(p, prot)	kmap_atomic(p)
#define	kmap_local_page_prot(p, prot)	kmap_local_page(p)
#define	kunmap(p)			do { (void)(p); } while (0)
#define	kunmap_atomic(v)		do { (void)(v); } while (0)
#define	kunmap_local(v)			do { (void)(v); } while (0)
#define	flush_dcache_page(p)		do { (void)(p); } while (0)
#define	flush_kernel_vmap_range(a, s)	do { (void)(a); } while (0)
#define	invalidate_kernel_vmap_range(a, s) do { (void)(a); } while (0)

static inline void
clear_highpage(struct page *p)
{
	memset(page_address(p), 0, PAGE_SIZE);
}

static inline void
memcpy_from_page(char *to, struct page *p, size_t off, size_t len)
{
	memcpy(to, (char *)page_address(p) + off, len);
}

static inline void
memcpy_to_page(struct page *p, size_t off, const char *from, size_t len)
{
	memcpy((char *)page_address(p) + off, from, len);
}

static inline void
memzero_page(struct page *p, size_t off, size_t len)
{
	memset((char *)page_address(p) + off, 0, len);
}

#endif /* _LINUXKPI_LINUX_HIGHMEM_H_ */
