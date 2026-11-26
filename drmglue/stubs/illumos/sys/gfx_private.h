/* SPDX-License-Identifier: BSD-2-Clause */
/* Stub for syntax checking only; see drmglue/stubs/README.md. */
#ifndef _STUB_SYS_GFX_PRIVATE_H
#define	_STUB_SYS_GFX_PRIVATE_H
#include <sys/types.h>
#include <sys/ddidevmap.h>
#define	GFXP_MEMORY_CACHED		0
#define	GFXP_MEMORY_UNCACHED		1
#define	GFXP_MEMORY_WRITECOMBINED	2
extern ddi_umem_cookie_t gfxp_umem_cookie_init(caddr_t, size_t);
extern void gfxp_umem_cookie_destroy(ddi_umem_cookie_t);
extern int gfxp_devmap_umem_setup(devmap_cookie_t, dev_info_t *,
    struct devmap_callback_ctl *, ddi_umem_cookie_t, offset_t, size_t,
    uint_t, uint_t, ddi_device_acc_attr_t *);
extern caddr_t gfxp_alloc_kernel_space(size_t);
extern void gfxp_free_kernel_space(caddr_t, size_t);
extern void gfxp_load_kernel_space(uint64_t, size_t, uint32_t, caddr_t);
extern void gfxp_unload_kernel_space(caddr_t, size_t);
#endif
