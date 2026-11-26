/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. struct page (renamed lkpi_page by <linuxkpi/platform.h>,
 * since illumos has its own struct page/page_t).
 *
 * A KPI page describes one page of physically contiguous kernel memory
 * allocated by alloc_pages() (src/linux_page.c); it is not the illumos
 * page_t. amd64 has no highmem, so every page has a permanent kernel
 * virtual address (lkpi_kva). For an order-n allocation the descriptors
 * form an array of 2^n entries and the first (head) holds the reference
 * count and the order.
 */
#ifndef _LINUXKPI_LINUX_MM_TYPES_H_
#define	_LINUXKPI_LINUX_MM_TYPES_H_

#include <linux/types.h>
#include <linux/list.h>
#include <linux/atomic.h>
#include <linux/pgtable.h>

struct address_space;

struct page {
	unsigned long		flags;
	struct list_head	lru;
	struct address_space	*mapping;
	pgoff_t			index;
	unsigned long		private;
	atomic_t		_refcount;
	atomic_t		_mapcount;
	/* KPI private */
	void			*lkpi_kva;
	phys_addr_t		lkpi_phys;
	struct page		*lkpi_head;
	unsigned int		lkpi_order;
	void			*lkpi_cookie;
};

struct folio {
	struct page		page;
};

#define	page_folio(p)		((struct folio *)(p))
#define	folio_page(f, n)	(&(f)->page + (n))

#endif /* _LINUXKPI_LINUX_MM_TYPES_H_ */
