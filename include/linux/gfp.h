/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation from the documented behaviour of the Linux
 * gfp API; flag set cross-checked against FreeBSD's LinuxKPI gfp.h
 * (BSD-2-Clause). Bit values are this project's own.
 *
 * Mapping onto kmem_alloc(9F) flags (lkpi_gfp_to_plat):
 *   __GFP_NOFAIL                         KM_SLEEP (never fails)
 *   no __GFP_DIRECT_RECLAIM (GFP_ATOMIC,
 *     GFP_NOWAIT)                        KM_NOSLEEP (may fail)
 *   __GFP_NORETRY or __GFP_RETRY_MAYFAIL KM_NOSLEEP | KM_NORMALPRI
 *   otherwise (GFP_KERNEL, ...)          KM_SLEEP (never fails)
 * Zone modifiers (__GFP_DMA32, __GFP_HIGHMEM, ...) have no meaning for
 * kmem and are ignored here; page allocation (P2) honours __GFP_DMA32.
 */
#ifndef _LINUXKPI_LINUX_GFP_H_
#define	_LINUXKPI_LINUX_GFP_H_

#include <linuxkpi/platform.h>
#include <linux/types.h>

#define	__GFP_DIRECT_RECLAIM	((gfp_t)0x00000001)
#define	__GFP_KSWAPD_RECLAIM	((gfp_t)0x00000002)
#define	__GFP_IO		((gfp_t)0x00000004)
#define	__GFP_FS		((gfp_t)0x00000008)
#define	__GFP_HIGH		((gfp_t)0x00000010)
#define	__GFP_ZERO		((gfp_t)0x00000020)
#define	__GFP_NOWARN		((gfp_t)0x00000040)
#define	__GFP_NORETRY		((gfp_t)0x00000080)
#define	__GFP_RETRY_MAYFAIL	((gfp_t)0x00000100)
#define	__GFP_NOFAIL		((gfp_t)0x00000200)
#define	__GFP_COMP		((gfp_t)0x00000400)
#define	__GFP_NOMEMALLOC	((gfp_t)0x00000800)
#define	__GFP_MEMALLOC		((gfp_t)0x00001000)
#define	__GFP_ACCOUNT		((gfp_t)0x00002000)
#define	__GFP_HARDWALL		((gfp_t)0x00004000)
#define	__GFP_THISNODE		((gfp_t)0x00008000)
#define	__GFP_RECLAIMABLE	((gfp_t)0x00010000)
#define	__GFP_MOVABLE		((gfp_t)0x00020000)
#define	__GFP_DMA		((gfp_t)0x00040000)
#define	__GFP_DMA32		((gfp_t)0x00080000)
#define	__GFP_HIGHMEM		((gfp_t)0x00100000)
#define	__GFP_NOLOCKDEP		((gfp_t)0x00200000)
#define	__GFP_RECLAIM		(__GFP_DIRECT_RECLAIM | __GFP_KSWAPD_RECLAIM)

#define	GFP_ATOMIC		(__GFP_HIGH | __GFP_KSWAPD_RECLAIM)
#define	GFP_NOWAIT		(__GFP_KSWAPD_RECLAIM)
#define	GFP_NOIO		(__GFP_RECLAIM)
#define	GFP_NOFS		(__GFP_RECLAIM | __GFP_IO)
#define	GFP_KERNEL		(__GFP_RECLAIM | __GFP_IO | __GFP_FS)
#define	GFP_KERNEL_ACCOUNT	(GFP_KERNEL | __GFP_ACCOUNT)
#define	GFP_USER		(GFP_KERNEL | __GFP_HARDWALL)
#define	GFP_HIGHUSER		(GFP_USER | __GFP_HIGHMEM)
#define	GFP_HIGHUSER_MOVABLE	(GFP_HIGHUSER | __GFP_MOVABLE)
#define	GFP_DMA			__GFP_DMA
#define	GFP_DMA32		__GFP_DMA32
#define	GFP_TRANSHUGE_LIGHT	(GFP_HIGHUSER_MOVABLE | __GFP_COMP | \
				    __GFP_NOMEMALLOC | __GFP_NOWARN)
#define	GFP_TRANSHUGE		(GFP_TRANSHUGE_LIGHT | __GFP_DIRECT_RECLAIM)
#define	GFP_ZONEMASK		(__GFP_DMA | __GFP_DMA32 | __GFP_HIGHMEM | \
				    __GFP_MOVABLE)

static inline bool
gfpflags_allow_blocking(const gfp_t gfp)
{
	return ((gfp & __GFP_DIRECT_RECLAIM) != 0);
}

static inline int
lkpi_gfp_to_plat(gfp_t gfp)
{
	if (gfp & __GFP_NOFAIL)
		return (LKPI_PLAT_SLEEP);
	if (!gfpflags_allow_blocking(gfp))
		return (LKPI_PLAT_NOSLEEP);
	if (gfp & (__GFP_NORETRY | __GFP_RETRY_MAYFAIL))
		return (LKPI_PLAT_NOSLEEP_LAZY);
	return (LKPI_PLAT_SLEEP);
}

/*
 * Size-aware mapping used by kmalloc/kvmalloc (docs/12 M2). illumos
 * KM_SLEEP never fails: it waits (and reaps system-wide) until memory
 * appears, while Linux fails an unsatisfiable GFP_KERNEL request. Large
 * requests are often user-sized (execbuffer arrays, property blobs), so
 * above LKPI_GFP_SLEEP_MAX they become KM_NOSLEEP | KM_NORMALPRI and can
 * fail; only __GFP_NOFAIL keeps KM_SLEEP at any size.
 */
#define	LKPI_GFP_SLEEP_MAX	(1UL << 20)

static inline int
lkpi_gfp_to_plat_size(gfp_t gfp, size_t size)
{
	int f = lkpi_gfp_to_plat(gfp);

	if (f == LKPI_PLAT_SLEEP && size > LKPI_GFP_SLEEP_MAX &&
	    !(gfp & __GFP_NOFAIL))
		return (LKPI_PLAT_NOSLEEP_LAZY);
	return (f);
}

#endif /* _LINUXKPI_LINUX_GFP_H_ */
