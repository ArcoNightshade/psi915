/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Page size and page protections. pgprot_t carries
 * protection bits and a cache mode; only the cache mode is honoured by
 * the KPI mapping functions (vmap(), and the DRM glue's devmap handles).
 * The definitions match drmglue's (which includes this header).
 */
#ifndef _LINUXKPI_LINUX_PGTABLE_H_
#define	_LINUXKPI_LINUX_PGTABLE_H_

#include <linux/types.h>

#ifndef PAGE_SHIFT
#define	PAGE_SHIFT	12
#endif
#ifndef PAGE_SIZE
#define	PAGE_SIZE	(1UL << PAGE_SHIFT)
#endif
#ifndef PAGE_MASK
#define	PAGE_MASK	(~(PAGE_SIZE - 1))
#endif
#define	PAGE_ALIGN(x)		(((x) + PAGE_SIZE - 1) & PAGE_MASK)
#define	PAGE_ALIGNED(x)		(((unsigned long)(x) & (PAGE_SIZE - 1)) == 0)
#define	offset_in_page(p)	((unsigned long)(p) & ~PAGE_MASK)
#define	PFN_UP(x)		(((x) + PAGE_SIZE - 1) >> PAGE_SHIFT)
#define	PFN_DOWN(x)		((x) >> PAGE_SHIFT)
#define	PFN_PHYS(x)		((phys_addr_t)(x) << PAGE_SHIFT)
#define	PHYS_PFN(x)		((unsigned long)((x) >> PAGE_SHIFT))

#define	LKPI_HAVE_PGPROT	1

enum lkpi_cache_mode {
	LKPI_CACHE_WB = 0,	/* write-back (vm_get_page_prot) */
	LKPI_CACHE_WC = 1,	/* write-combining (pgprot_writecombine) */
	LKPI_CACHE_UC = 2	/* uncached (pgprot_noncached) */
};

typedef struct {
	unsigned long	pgprot;
} pgprot_t;

#define	LKPI_PGPROT_CACHE_MASK	0x3UL
#define	LKPI_PGPROT_READ	0x10UL
#define	LKPI_PGPROT_WRITE	0x20UL
#define	LKPI_PGPROT_EXEC	0x40UL

#define	__pgprot(x)		((pgprot_t){ (x) })
#define	pgprot_val(p)		((p).pgprot)

static inline enum lkpi_cache_mode
lkpi_pgprot_cache(pgprot_t p)
{
	return ((enum lkpi_cache_mode)(p.pgprot & LKPI_PGPROT_CACHE_MASK));
}

static inline pgprot_t
lkpi_pgprot_set_cache(pgprot_t p, enum lkpi_cache_mode c)
{
	return (__pgprot((p.pgprot & ~LKPI_PGPROT_CACHE_MASK) |
	    (unsigned long)c));
}

#define	pgprot_writecombine(p)	lkpi_pgprot_set_cache((p), LKPI_CACHE_WC)
#define	pgprot_noncached(p)	lkpi_pgprot_set_cache((p), LKPI_CACHE_UC)
#define	pgprot_device(p)	lkpi_pgprot_set_cache((p), LKPI_CACHE_UC)
#define	pgprot_decrypted(p)	(p)
#define	PAGE_KERNEL		__pgprot(LKPI_PGPROT_READ | LKPI_PGPROT_WRITE)
#define	PAGE_KERNEL_IO		PAGE_KERNEL
#define	PAGE_KERNEL_RO		__pgprot(LKPI_PGPROT_READ)
#define	PAGE_KERNEL_EXEC						\
	__pgprot(LKPI_PGPROT_READ | LKPI_PGPROT_WRITE | LKPI_PGPROT_EXEC)

#endif /* _LINUXKPI_LINUX_PGTABLE_H_ */
