/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * illumos devmap/segdev backend of the VMA emulation (docs/10, section 3).
 *
 * UNTESTED ON ILLUMOS: compile-checked against drmglue/stubs only. The
 * segdev properties this relies on were read in illumos-gate
 * usr/src/uts/common/vm/seg_dev.c (f71cb08e8c53) and are listed in
 * docs/10, section 3.7 as items to verify on a real system.
 *
 * Per VMA: a kernel window (gfx_private gfxp_alloc_kernel_space, no
 * translations) wrapped in a KMEM_NON_PAGEABLE umem cookie
 * (gfxp_umem_cookie_init), mapped to the user with gfxp_devmap_umem_setup.
 * segdev_faultpage() resolves user faults through hat_getpfnum(kas,
 * window + off), so loading a window page with gfxp_load_kernel_space()
 * is how a Linux pfn insertion becomes visible to devmap_load(). This is
 * the gfx-drm GTT mmap scheme (drm_gem.c gtt_map_kaddr) generalised to
 * per-page, per-VMA windows with arbitrary pfns.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/cred.h>
#include <sys/mman.h>
#include <sys/file.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/ddidevmap.h>
#include <sys/gfx_private.h>
#include <vm/as.h>
#include <vm/faultcode.h>
#include <vm/seg_enum.h>

#include <linuxkpi/platform.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <drmglue/lkpi_fs.h>
#include <drmglue/lkpi_mm.h>
#include <drmglue/backend.h>
#include <drmglue/glue.h>
#include <drmglue/illumos.h>

/* Synthetic vm_start: devmap(9E) runs before the user address is chosen. */
#define	DRMGLUE_VMA_BASE	0x10000000UL

struct drmglue_win {
	caddr_t			kva;
	size_t			len;
	ddi_umem_cookie_t	cookie;
};

static int
drmglue_win_create(struct lkpi_vma *lv, void **winp)
{
	struct drmglue_win *w;

	w = kmem_zalloc(sizeof (*w), KM_SLEEP);
	w->len = ptob(lv->npages);
	w->kva = gfxp_alloc_kernel_space(w->len);	/* VM_NOSLEEP inside */
	if (w->kva == NULL) {
		kmem_free(w, sizeof (*w));
		return (ENOMEM);
	}
	w->cookie = gfxp_umem_cookie_init(w->kva, w->len);
	if (w->cookie == NULL) {
		gfxp_free_kernel_space(w->kva, w->len);
		kmem_free(w, sizeof (*w));
		return (ENOMEM);
	}
	*winp = w;
	return (0);
}

static void
drmglue_win_destroy(struct lkpi_vma *lv, void *win)
{
	struct drmglue_win *w = win;

	gfxp_umem_cookie_destroy(w->cookie);
	gfxp_free_kernel_space(w->kva, w->len);
	kmem_free(w, sizeof (*w));
}

static uint32_t
drmglue_gfxp_mode(enum lkpi_cache_mode c)
{
	switch (c) {
	case LKPI_CACHE_WC:
		return (GFXP_MEMORY_WRITECOMBINED);
	case LKPI_CACHE_UC:
		return (GFXP_MEMORY_UNCACHED);
	default:
		return (GFXP_MEMORY_CACHED);
	}
}

static int
drmglue_win_load(void *win, unsigned long pg, unsigned long pfn,
    enum lkpi_cache_mode c)
{
	struct drmglue_win *w = win;

	/* hat_devload(kas, ..., HAT_LOAD_LOCK) of one page. */
	gfxp_load_kernel_space((uint64_t)pfn << PAGESHIFT, PAGESIZE,
	    drmglue_gfxp_mode(c), w->kva + ptob(pg));
	return (0);
}

static void
drmglue_win_unload(void *win, unsigned long pg, unsigned long n)
{
	struct drmglue_win *w = win;

	/* hat_unload(kas, ..., HAT_UNLOAD_UNLOCK); loaded pages only. */
	gfxp_unload_kernel_space(w->kva + ptob(pg), ptob(n));
}

static int
drmglue_user_load(void *seg, uint64_t off, size_t len, int type, bool write)
{
	enum fault_type ft;
	faultcode_t fc;

	switch (type) {
	case LKPI_FAULT_SOFTLOCK:
		ft = F_SOFTLOCK;
		break;
	case LKPI_FAULT_PROT:
		ft = F_PROT;
		break;
	default:
		ft = F_INVAL;
		break;
	}
	fc = devmap_load((devmap_cookie_t)seg, (offset_t)off, len, ft,
	    write ? S_WRITE : S_READ);
	if (fc == 0)
		return (0);
	return (FC_CODE(fc) == FC_OBJERR ? FC_ERRNO(fc) : EFAULT);
}

static void
drmglue_user_unload(void *seg, uint64_t off, size_t len)
{
	/* hat_unload of the user range; takes no segdev locks. */
	(void) devmap_unload((devmap_cookie_t)seg, (offset_t)off, len);
}

/*
 * devmap callbacks
 */
static int
drmglue_dm_map(devmap_cookie_t dhc, dev_t dev, uint_t flags, offset_t off,
    size_t len, void **pvtp)
{
	struct lkpi_file *f;
	struct lkpi_vma *lv = NULL;

	f = drmglue_dev_file(dev);
	if (f == NULL)
		return (ENXIO);
	/* Same thread as devmap(9E): f_lkpi_mmap_lock is held. */
	if (f->f_lkpi_mmap != NULL) {
		lv = f->f_lkpi_mmap->lv;
		f->f_lkpi_mmap->lv = NULL;
	}
	fput(f);
	if (lv == NULL)
		return (ENXIO);
	lkpi_vma_bind(lv, dhc);
	*pvtp = lv;
	return (0);
}

static int
drmglue_dm_access(devmap_cookie_t dhc, void *pvt, offset_t off, size_t len,
    uint_t type, uint_t rw)
{
	int t;

	switch (type) {
	case F_SOFTLOCK:
		t = LKPI_FAULT_SOFTLOCK;
		break;
	case F_PROT:
		t = LKPI_FAULT_PROT;
		break;
	default:
		t = LKPI_FAULT_INVAL;
		break;
	}
	return (lkpi_vma_fault(pvt, (uint64_t)off, len, t, rw == S_WRITE));
}

static int
drmglue_dm_dup(devmap_cookie_t dhc, void *pvt, devmap_cookie_t new_dhc,
    void **new_pvtp)
{
	struct lkpi_vma *nlv = NULL;
	int r;

	r = lkpi_vma_dup(pvt, new_dhc, &nlv);
	*new_pvtp = nlv;
	return (r);
}

static void
drmglue_dm_unmap(devmap_cookie_t dhc, void *pvt, offset_t off, size_t len,
    devmap_cookie_t new_dhp1, void **new_pvtp1, devmap_cookie_t new_dhp2,
    void **new_pvtp2)
{
	struct lkpi_vma *n1 = NULL, *n2 = NULL;

	lkpi_vma_unmap(pvt, (uint64_t)off, len, new_dhp1, &n1, new_dhp2, &n2);
	if (new_pvtp1 != NULL)
		*new_pvtp1 = n1;
	if (new_pvtp2 != NULL)
		*new_pvtp2 = n2;
}

static struct devmap_callback_ctl drmglue_devmap_cb = {
	.devmap_rev = DEVMAP_OPS_REV,
	.devmap_map = drmglue_dm_map,
	.devmap_access = drmglue_dm_access,
	.devmap_dup = drmglue_dm_dup,
	.devmap_unmap = drmglue_dm_unmap
};

static unsigned int
drmglue_lprot(uint_t prot)
{
	return (((prot & PROT_READ) ? LKPI_PROT_READ : 0) |
	    ((prot & PROT_WRITE) ? LKPI_PROT_WRITE : 0) |
	    ((prot & PROT_EXEC) ? LKPI_PROT_EXEC : 0));
}

/*
 * devmap(9E) body for one file: create the VMA (runs f_op->mmap) and set
 * up the devmap handle over its window with the VMA's cache attribute.
 */
int
drmglue_devmap_file(struct lkpi_file *f, dev_info_t *dip,
    devmap_cookie_t dhp, offset_t off, size_t len, size_t *maplen)
{
	struct lkpi_mmap_req *req = f->f_lkpi_mmap;
	ddi_device_acc_attr_t acc;
	struct drmglue_win *w;
	struct lkpi_vma *lv;
	int r;

	if (req == NULL || req->lv != NULL)
		return (ENXIO);	/* not entered through our segmap */
	len = ptob(btopr(len));
	r = lkpi_vma_create(f, (uint64_t)off, len, drmglue_lprot(req->prot),
	    drmglue_lprot(req->maxprot), LKPI_MAP_SHARED, DRMGLUE_VMA_BASE,
	    &lv);
	if (r != 0)
		return (r);

	bzero(&acc, sizeof (acc));
	acc.devacc_attr_version = DDI_DEVICE_ATTR_V0;
	acc.devacc_attr_endian_flags = DDI_NEVERSWAP_ACC;
	switch (lv->cache) {
	case LKPI_CACHE_WC:
		acc.devacc_attr_dataorder = DDI_MERGING_OK_ACC;
		break;
	case LKPI_CACHE_UC:
		acc.devacc_attr_dataorder = DDI_STRICTORDER_ACC;
		break;
	default:
		acc.devacc_attr_dataorder = DDI_STORECACHING_OK_ACC;
		break;
	}
	w = lv->win;
	r = gfxp_devmap_umem_setup(dhp, dip, &drmglue_devmap_cb, w->cookie, 0,
	    len, req->maxprot, 0, &acc);
	if (r != DDI_SUCCESS) {
		lkpi_vma_destroy(lv);
		return (ENXIO);
	}
	req->lv = lv;		/* claimed by drmglue_dm_map */
	*maplen = len;
	return (0);
}

/*
 * segmap(9E) body (and anonymous VOP_MAP): record the request, let
 * devmap_setup() call devmap(9E) and the devmap_map callback in this
 * thread, and destroy the VMA if setup failed between the two.
 */
int
drmglue_segmap_file(struct lkpi_file *f, dev_t dev, offset_t off,
    struct as *as, caddr_t *addrp, size_t len, uint_t prot, uint_t maxprot,
    uint_t flags, cred_t *cr)
{
	struct lkpi_mmap_req req;
	int r;

	if ((flags & MAP_TYPE) != MAP_SHARED)
		return (EINVAL);
	if ((prot & PROT_WRITE) && !(f->f_mode & FMODE_WRITE))
		return (EACCES);
	if (!(f->f_mode & FMODE_WRITE))
		maxprot &= ~PROT_WRITE;

	mutex_lock(&f->f_lkpi_mmap_lock);
	req.prot = prot;
	req.maxprot = maxprot;
	req.flags = flags;
	req.lv = NULL;
	f->f_lkpi_mmap = &req;
	r = devmap_setup(dev, off, as, addrp, len, prot, maxprot, flags, cr);
	f->f_lkpi_mmap = NULL;
	mutex_unlock(&f->f_lkpi_mmap_lock);
	if (req.lv != NULL)
		lkpi_vma_destroy(req.lv);
	return (r);
}

void
drmglue_seg_be_fill(struct lkpi_glue_backend *be)
{
	be->win_create = drmglue_win_create;
	be->win_destroy = drmglue_win_destroy;
	be->win_load = drmglue_win_load;
	be->win_unload = drmglue_win_unload;
	be->user_load = drmglue_user_load;
	be->user_unload = drmglue_user_unload;
}
