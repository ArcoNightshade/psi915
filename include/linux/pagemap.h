/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Page-cache helpers used by GEM shmem objects (mapping flags are
 * recorded only; the shmem backend is the DRM glue's, P3).
 */
#ifndef _LINUXKPI_INCLUDE_LINUX_PAGEMAP_H_
#define	_LINUXKPI_INCLUDE_LINUX_PAGEMAP_H_

#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/highmem.h>

#define	mapping_set_unevictable(m)	do { (void)(m); } while (0)
#define	mapping_clear_unevictable(m)	do { (void)(m); } while (0)
#define	mapping_unevictable(m)		((void)(m), false)
#define	mapping_gfp_mask(m)		((void)(m), GFP_KERNEL)
#define	mapping_gfp_constraint(m, g)	((void)(m), (g))
#define	mapping_set_gfp_mask(m, g)	do { (void)(m); (void)(g); } while (0)
#define	mapping_set_large_folios(m)	do { (void)(m); } while (0)
#define	folio_mark_dirty(f)		((void)(f), false)
#define	folio_mark_accessed(f)		do { (void)(f); } while (0)
#define	folio_mapped(f)			((void)(f), false)
#define	folio_nr_pages(f)		(1UL << compound_order(&(f)->page))
#define	folio_size(f)			(PAGE_SIZE * folio_nr_pages(f))
#define	folio_test_highmem(f)		(false)
#define	folio_pfn(f)			page_to_pfn(&(f)->page)
#define	folio_lock(f)			do { (void)(f); } while (0)
#define	folio_unlock(f)			do { (void)(f); } while (0)

#endif /* _LINUXKPI_INCLUDE_LINUX_PAGEMAP_H_ */
