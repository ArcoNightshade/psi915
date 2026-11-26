/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Stub for HOST_STUBS=1 syntax checking of drivers/i915_glue only (not a
 * copy of the illumos header, not ABI-accurate). Union of what drmglue and
 * the i915 glue use; prototypes follow illumos-gate f71cb08e8c53
 * usr/src/uts/common/sys/gfx_private.h.
 */
#ifndef _STUB_SYS_GFX_PRIVATE_H
#define	_STUB_SYS_GFX_PRIVATE_H
#include <sys/types.h>
#include <sys/ddidevmap.h>
#include <sys/devops.h>
#define	GFXP_MEMORY_CACHED		0
#define	GFXP_MEMORY_UNCACHED		1
#define	GFXP_MEMORY_WRITECOMBINED	2
typedef char *gfxp_kva_t;
typedef char *gfxp_fb_softc_ptr_t;
struct cred;
extern ddi_umem_cookie_t gfxp_umem_cookie_init(caddr_t, size_t);
extern void gfxp_umem_cookie_destroy(ddi_umem_cookie_t);
extern int gfxp_devmap_umem_setup(devmap_cookie_t, dev_info_t *,
    struct devmap_callback_ctl *, ddi_umem_cookie_t, offset_t, size_t,
    uint_t, uint_t, ddi_device_acc_attr_t *);
extern caddr_t gfxp_alloc_kernel_space(size_t);
extern void gfxp_free_kernel_space(caddr_t, size_t);
extern void gfxp_load_kernel_space(uint64_t, size_t, uint32_t, caddr_t);
extern void gfxp_unload_kernel_space(caddr_t, size_t);
extern gfxp_kva_t gfxp_map_kernel_space(uint64_t, size_t, uint32_t);
extern void gfxp_unmap_kernel_space(gfxp_kva_t, size_t);
extern gfxp_fb_softc_ptr_t gfxp_fb_softc_alloc(void);
extern void gfxp_fb_softc_free(gfxp_fb_softc_ptr_t);
extern int gfxp_fb_attach(dev_info_t *, ddi_attach_cmd_t, gfxp_fb_softc_ptr_t);
extern int gfxp_fb_detach(dev_info_t *, ddi_detach_cmd_t, gfxp_fb_softc_ptr_t);
extern int gfxp_fb_open(dev_t *, int, int, struct cred *, gfxp_fb_softc_ptr_t);
extern int gfxp_fb_close(dev_t, int, int, struct cred *, gfxp_fb_softc_ptr_t);
extern int gfxp_fb_ioctl(dev_t, int, intptr_t, int, struct cred *, int *,
    gfxp_fb_softc_ptr_t);
extern int gfxp_fb_devmap(dev_t, devmap_cookie_t, offset_t, size_t, size_t *,
    uint_t, void *);
struct gfxp_blt_ops {
	int (*blt)(void *);
	int (*copy)(void *);
	int (*clear)(void *);
	int (*setmode)(int);
};
extern void gfxp_bm_register_fbops(gfxp_fb_softc_ptr_t, struct gfxp_blt_ops *);
#endif
