/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for syntax checking only; see tests/stubs/illumos/README.md. */
#ifndef _STUB_SYS_GFX_PRIVATE_H
#define	_STUB_SYS_GFX_PRIVATE_H
#include <sys/types.h>
#define	GFXP_MEMORY_CACHED		0
#define	GFXP_MEMORY_UNCACHED		1
#define	GFXP_MEMORY_WRITECOMBINED	2
typedef char *gfxp_kva_t;
extern gfxp_kva_t gfxp_map_kernel_space(uint64_t, size_t, uint32_t);
extern void gfxp_unmap_kernel_space(gfxp_kva_t, size_t);
#endif
