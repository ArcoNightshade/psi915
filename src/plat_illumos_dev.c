/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * illumos hardware backend of the LinuxKPI (see <linuxkpi/plat_dev.h>).
 *
 * UNTESTED ON ILLUMOS: written against illumos-gate headers (f71cb08e8c53)
 * and the DDI manual pages, compile-checked only against the stubs in
 * tests/stubs/illumos. Points to verify first on hardware are marked
 * "VERIFY".
 *
 * PCI "reg"/"assigned-addresses" entries follow the IEEE 1275 PCI binding
 * (pci_regspec_t, 5 cells): phys_hi = npt000ss bbbbbbbb dddddfff rrrrrrrr
 * (ss: 00 config, 01 I/O, 10 mem32, 11 mem64; p: prefetchable; b/d/f:
 * bus/device/function; r: config register of the BAR), then phys_mid,
 * phys_low, size_hi, size_low. ddi_regs_map_setup()'s rnumber is the
 * index into "reg".
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/kmem.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/sunndi.h>
#include <sys/ddi_intr.h>
#include <sys/ddidmareq.h>
#include <sys/kobj.h>
#include <sys/sysevent.h>
#include <sys/nvpair.h>
#include <sys/smbios.h>
#include <sys/gfx_private.h>
#include <sys/x86_archext.h>
#include <sys/acpi/acpi.h>
#include <sys/acpica.h>
#include <vm/hat.h>
#include <vm/as.h>
#include <vm/seg_kmem.h>
#include <sys/vmem.h>
#include <sys/buf.h>
#include <sys/x_call.h>
#include <sys/cpuvar.h>
#include <sys/disp.h>
#include <vm/page.h>

#include <linuxkpi/platform.h>

#ifdef LINUXKPI_ILLUMOS_KERNEL

#include <linuxkpi/plat_dev.h>
#include <linux/errno.h>

#define	REG_SPACE(hi)		(((hi) >> 24) & 0x3)
#define	REG_PREFETCH(hi)	(((hi) >> 30) & 0x1)
#define	REG_BUS(hi)		(((hi) >> 16) & 0xff)
#define	REG_DEVFN(hi)		(((hi) >> 8) & 0xff)
#define	REG_OFF(hi)		((hi) & 0xff)
#define	SPACE_CONFIG		0
#define	SPACE_IO		1
#define	SPACE_MEM32		2
#define	SPACE_MEM64		3

static int
lkpi_prop_int(dev_info_t *dip, const char *name, int def)
{
	return (ddi_prop_get_int(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
	    (char *)name, def));
}

int
lkpi_plat_pci_info(void *dipp, struct lkpi_plat_pci_info *info)
{
	dev_info_t *dip = dipp;
	int *regs = NULL, *aa = NULL;
	uint_t nregs = 0, naa = 0, i, j;
	int classcode;

	memset(info, 0, sizeof (*info));
	info->vendor = (u16)lkpi_prop_int(dip, "vendor-id", 0xffff);
	info->device = (u16)lkpi_prop_int(dip, "device-id", 0xffff);
	info->subvendor = (u16)lkpi_prop_int(dip, "subsystem-vendor-id", 0);
	info->subdevice = (u16)lkpi_prop_int(dip, "subsystem-id", 0);
	info->revision = (u8)lkpi_prop_int(dip, "revision-id", 0);
	classcode = lkpi_prop_int(dip, "class-code", 0);
	info->class = (u32)classcode & 0xffffff;
	for (i = 0; i < LKPI_PCI_NUM_BARS; i++)
		info->bar[i].rnumber = -1;

	if (ddi_prop_lookup_int_array(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
	    "reg", &regs, &nregs) != DDI_PROP_SUCCESS || nregs < 5)
		return (-ENXIO);
	info->bus = (u8)REG_BUS((u32)regs[0]);
	info->devfn = (u8)REG_DEVFN((u32)regs[0]);
	/* VERIFY: "pci-segment" is not standard; domain 0 is assumed. */
	info->domain = 0;

	(void) ddi_prop_lookup_int_array(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
	    "assigned-addresses", &aa, &naa);

	for (i = 0; i + 5 <= nregs; i += 5) {
		u32 hi = (u32)regs[i];
		int off = (int)REG_OFF(hi), bar;

		if (REG_SPACE(hi) == SPACE_CONFIG)
			continue;
		if (off == 0x30) {
			/* Expansion ROM */
			for (j = 0; aa != NULL && j + 5 <= naa; j += 5) {
				if (REG_OFF((u32)aa[j]) == 0x30) {
					info->rom_start = ((u64)(u32)aa[j + 1] << 32) |
					    (u32)aa[j + 2];
					info->rom_len = ((u64)(u32)aa[j + 3] << 32) |
					    (u32)aa[j + 4];
				}
			}
			continue;
		}
		if (off < 0x10 || off > 0x24 || (off & 3) != 0)
			continue;
		bar = (off - 0x10) / 4;
		info->bar[bar].rnumber = (int)(i / 5);
		info->bar[bar].len = ((u64)(u32)regs[i + 3] << 32) |
		    (u32)regs[i + 4];
		switch (REG_SPACE(hi)) {
		case SPACE_IO:
			info->bar[bar].flags = LKPI_BAR_IO;
			break;
		case SPACE_MEM64:
			info->bar[bar].flags = LKPI_BAR_MEM | LKPI_BAR_64;
			break;
		default:
			info->bar[bar].flags = LKPI_BAR_MEM;
			break;
		}
		if (REG_PREFETCH(hi))
			info->bar[bar].flags |= LKPI_BAR_PREFETCH;
		for (j = 0; aa != NULL && j + 5 <= naa; j += 5) {
			if (REG_OFF((u32)aa[j]) == (u32)off) {
				info->bar[bar].start =
				    ((u64)(u32)aa[j + 1] << 32) | (u32)aa[j + 2];
				break;
			}
		}
	}
	if (aa != NULL)
		ddi_prop_free(aa);
	ddi_prop_free(regs);
	return (0);
}

int
lkpi_plat_pci_cfg_open(void *dip, void **cfgh)
{
	ddi_acc_handle_t h;

	if (pci_config_setup(dip, &h) != DDI_SUCCESS)
		return (-EIO);
	*cfgh = h;
	return (0);
}

void
lkpi_plat_pci_cfg_close(void *cfgh)
{
	ddi_acc_handle_t h = cfgh;

	pci_config_teardown(&h);
}

u32
lkpi_plat_pci_cfg_read(void *cfgh, int where, int size)
{
	switch (size) {
	case 1:
		return (pci_config_get8(cfgh, (off_t)where));
	case 2:
		return (pci_config_get16(cfgh, (off_t)where));
	default:
		return (pci_config_get32(cfgh, (off_t)where));
	}
}

void
lkpi_plat_pci_cfg_write(void *cfgh, int where, int size, u32 val)
{
	switch (size) {
	case 1:
		pci_config_put8(cfgh, (off_t)where, (uint8_t)val);
		break;
	case 2:
		pci_config_put16(cfgh, (off_t)where, (uint16_t)val);
		break;
	default:
		pci_config_put32(cfgh, (off_t)where, val);
		break;
	}
}

struct lkpi_walk_arg {
	int	(*fn)(void *, void *);
	void	*arg;
};

static int
lkpi_walk_cb(dev_info_t *dip, void *a)
{
	struct lkpi_walk_arg *wa = a;
	int r;

	if (ddi_prop_exists(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
	    "vendor-id") == 0 ||
	    ddi_prop_exists(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
	    "reg") == 0)
		return (DDI_WALK_CONTINUE);
	ndi_hold_devi(dip);
	r = wa->fn(dip, wa->arg);
	ndi_rele_devi(dip);
	return (r != 0 ? DDI_WALK_TERMINATE : DDI_WALK_CONTINUE);
}

/*
 * VERIFY: ddi_walk_devs(9F) from a leaf driver's attach(9E) on the root
 * node; the manual allows it when no locks are held.
 */
void
lkpi_plat_pci_walk(int (*fn)(void *dip, void *arg), void *arg)
{
	struct lkpi_walk_arg wa = { fn, arg };

	ddi_walk_devs(ddi_root_node(), lkpi_walk_cb, &wa);
}

void
lkpi_plat_dev_hold(void *dip)
{
	ndi_hold_devi(dip);
}

void
lkpi_plat_dev_rele(void *dip)
{
	ndi_rele_devi(dip);
}

void *
lkpi_plat_dev_parent(void *dip)
{
	return (ddi_get_parent(dip));
}

const char *
lkpi_plat_dev_name(void *dip)
{
	return (ddi_node_name(dip));
}

static void
lkpi_acc_attr(ddi_device_acc_attr_t *attr, int cache)
{
	attr->devacc_attr_version = DDI_DEVICE_ATTR_V0;
	attr->devacc_attr_endian_flags = DDI_NEVERSWAP_ACC;
	attr->devacc_attr_dataorder = cache == LKPI_CACHE_ATTR_WC ?
	    DDI_MERGING_OK_ACC : (cache == LKPI_CACHE_ATTR_WB ?
	    DDI_STORECACHING_OK_ACC : DDI_STRICTORDER_ACC);
	attr->devacc_attr_access = DDI_DEFAULT_ACC;
}

void *
lkpi_plat_map_regs(void *dip, int rnumber, u64 off, u64 len, int cache,
    void **handle)
{
	ddi_device_acc_attr_t attr;
	ddi_acc_handle_t h;
	caddr_t va;

	lkpi_acc_attr(&attr, cache);
	if (ddi_regs_map_setup(dip, (uint_t)rnumber, &va, (offset_t)off,
	    (offset_t)len, &attr, &h) != DDI_SUCCESS)
		return (NULL);
	*handle = h;
	return (va);
}

void
lkpi_plat_unmap_regs(void **handle)
{
	ddi_acc_handle_t h = *handle;

	if (h != NULL)
		ddi_regs_map_free(&h);
	*handle = NULL;
}

static uint32_t
lkpi_gfxp_mode(int cache)
{
	switch (cache) {
	case LKPI_CACHE_ATTR_WC:
		return (GFXP_MEMORY_WRITECOMBINED);
	case LKPI_CACHE_ATTR_WB:
		return (GFXP_MEMORY_CACHED);
	default:
		return (GFXP_MEMORY_UNCACHED);
	}
}

void *
lkpi_plat_map_phys(u64 phys, size_t len, int cache)
{
	return (gfxp_map_kernel_space(phys, len, lkpi_gfxp_mode(cache)));
}

void
lkpi_plat_unmap_phys(void *va, size_t len)
{
	gfxp_unmap_kernel_space(va, len);
}

/* Interrupts. */

struct lkpi_plat_intr {
	dev_info_t		*dip;
	int			type;
	int			count;
	int			cap;
	uint_t			pri;
	ddi_intr_handle_t	*h;
	size_t			hsize;
	bool			(**fn)(void *);
	void			**arg;
};

static uint_t
lkpi_intr_handler(caddr_t a1, caddr_t a2)
{
	struct lkpi_plat_intr *ip = (struct lkpi_plat_intr *)a1;
	int idx = (int)(uintptr_t)a2;

	return (ip->fn[idx](ip->arg[idx]) ? DDI_INTR_CLAIMED :
	    DDI_INTR_UNCLAIMED);
}

int
lkpi_plat_intr_alloc(void *dipp, unsigned int types, int min, int max,
    struct lkpi_plat_intr **ipp, unsigned int *typep, int *countp)
{
	static const struct { unsigned int lkpi; int ddi; } order[] = {
		{ LKPI_INTR_MSIX, DDI_INTR_TYPE_MSIX },
		{ LKPI_INTR_MSI, DDI_INTR_TYPE_MSI },
		{ LKPI_INTR_INTX, DDI_INTR_TYPE_FIXED },
	};
	dev_info_t *dip = dipp;
	struct lkpi_plat_intr *ip;
	int supported, i, navail, actual;

	if (ddi_intr_get_supported_types(dip, &supported) != DDI_SUCCESS)
		return (-ENODEV);
	for (i = 0; i < 3; i++) {
		if (!(types & order[i].lkpi) || !(supported & order[i].ddi))
			continue;
		if (ddi_intr_get_navail(dip, order[i].ddi, &navail) !=
		    DDI_SUCCESS || navail < min)
			continue;
		if (navail > max)
			navail = max;
		ip = kmem_zalloc(sizeof (*ip), KM_SLEEP);
		ip->dip = dip;
		ip->type = order[i].ddi;
		ip->hsize = (size_t)navail * sizeof (ddi_intr_handle_t);
		ip->h = kmem_zalloc(ip->hsize, KM_SLEEP);
		if (ddi_intr_alloc(dip, ip->h, ip->type, 0, navail, &actual,
		    DDI_INTR_ALLOC_NORMAL) != DDI_SUCCESS || actual < min) {
			int k;

			for (k = 0; k < actual; k++)
				(void) ddi_intr_free(ip->h[k]);
			kmem_free(ip->h, ip->hsize);
			kmem_free(ip, sizeof (*ip));
			continue;
		}
		ip->count = actual;
		(void) ddi_intr_get_cap(ip->h[0], &ip->cap);
		(void) ddi_intr_get_pri(ip->h[0], &ip->pri);
		ip->fn = kmem_zalloc(sizeof (*ip->fn) * (size_t)actual,
		    KM_SLEEP);
		ip->arg = kmem_zalloc(sizeof (void *) * (size_t)actual,
		    KM_SLEEP);
		*ipp = ip;
		*typep = order[i].lkpi;
		*countp = actual;
		return (0);
	}
	return (-ENOSPC);
}

void
lkpi_plat_intr_free(struct lkpi_plat_intr *ip)
{
	int i;

	for (i = 0; i < ip->count; i++)
		(void) ddi_intr_free(ip->h[i]);
	kmem_free(ip->fn, sizeof (*ip->fn) * (size_t)ip->count);
	kmem_free(ip->arg, sizeof (void *) * (size_t)ip->count);
	kmem_free(ip->h, ip->hsize);
	kmem_free(ip, sizeof (*ip));
}

int
lkpi_plat_intr_add(struct lkpi_plat_intr *ip, int idx,
    bool (*fn)(void *arg), void *arg)
{
	ip->fn[idx] = fn;
	ip->arg[idx] = arg;
	if (ddi_intr_add_handler(ip->h[idx], lkpi_intr_handler,
	    (caddr_t)ip, (caddr_t)(uintptr_t)idx) != DDI_SUCCESS) {
		ip->fn[idx] = NULL;
		return (-EIO);
	}
	return (0);
}

void
lkpi_plat_intr_remove(struct lkpi_plat_intr *ip, int idx)
{
	(void) ddi_intr_remove_handler(ip->h[idx]);
	ip->fn[idx] = NULL;
	ip->arg[idx] = NULL;
}

/* VERIFY: block-enable capable MSI with a single handle. */
int
lkpi_plat_intr_enable(struct lkpi_plat_intr *ip, int idx)
{
	int r;

	if (ip->cap & DDI_INTR_FLAG_BLOCK)
		r = ddi_intr_block_enable(&ip->h[idx], 1);
	else
		r = ddi_intr_enable(ip->h[idx]);
	return (r == DDI_SUCCESS ? 0 : -EIO);
}

int
lkpi_plat_intr_disable(struct lkpi_plat_intr *ip, int idx)
{
	int r;

	if (ip->cap & DDI_INTR_FLAG_BLOCK)
		r = ddi_intr_block_disable(&ip->h[idx], 1);
	else
		r = ddi_intr_disable(ip->h[idx]);
	return (r == DDI_SUCCESS ? 0 : -EIO);
}

void *
lkpi_plat_intr_iblock(struct lkpi_plat_intr *ip, int idx)
{
	(void) idx;
	return (DDI_INTR_PRI(ip->pri));
}

bool
lkpi_plat_intr_hilevel(struct lkpi_plat_intr *ip, int idx)
{
	(void) idx;
	return (ip->pri >= ddi_intr_get_hilevel_pri());
}

/* DMA. */

struct lkpi_plat_dma {
	ddi_dma_handle_t	dmah;
	ddi_acc_handle_t	acch;	/* coherent allocations only */
	caddr_t			kva;
	size_t			real_len;
	/* lkpi_plat_dma_map_pages: buf and page_t list kept while bound */
	struct buf		*bp;
	page_t			**pplist;
	size_t			npages;
};

static void
lkpi_dma_attr(ddi_dma_attr_t *a, u64 mask, u64 align)
{
	memset(a, 0, sizeof (*a));
	a->dma_attr_version = DMA_ATTR_V0;
	a->dma_attr_addr_lo = 0;
	a->dma_attr_addr_hi = mask;
	a->dma_attr_count_max = mask;
	a->dma_attr_align = align;
	a->dma_attr_burstsizes = 0x7ff;
	a->dma_attr_minxfer = 1;
	a->dma_attr_maxxfer = mask;
	a->dma_attr_seg = ~0ULL;
	a->dma_attr_sgllen = 1;
	a->dma_attr_granular = 1;
	a->dma_attr_flags = 0;
}

int
lkpi_plat_dma_alloc_coherent(void *dip, u64 mask, size_t size, void **kva,
    u64 *bus, struct lkpi_plat_dma **hp)
{
	ddi_device_acc_attr_t acc;
	ddi_dma_attr_t attr;
	ddi_dma_cookie_t cookie;
	struct lkpi_plat_dma *h;
	uint_t ccount;

	h = kmem_zalloc(sizeof (*h), KM_SLEEP);
	lkpi_dma_attr(&attr, mask, PAGESIZE);
	lkpi_acc_attr(&acc, LKPI_CACHE_ATTR_WB);
	if (ddi_dma_alloc_handle(dip, &attr, DDI_DMA_SLEEP, NULL,
	    &h->dmah) != DDI_SUCCESS)
		goto fail;
	if (ddi_dma_mem_alloc(h->dmah, size, &acc, DDI_DMA_CONSISTENT,
	    DDI_DMA_SLEEP, NULL, &h->kva, &h->real_len, &h->acch) !=
	    DDI_SUCCESS)
		goto fail_handle;
	if (ddi_dma_addr_bind_handle(h->dmah, NULL, h->kva, h->real_len,
	    DDI_DMA_RDWR | DDI_DMA_CONSISTENT, DDI_DMA_SLEEP, NULL, &cookie,
	    &ccount) != DDI_DMA_MAPPED)
		goto fail_mem;
	if (ccount != 1) {
		(void) ddi_dma_unbind_handle(h->dmah);
		goto fail_mem;
	}
	bzero(h->kva, h->real_len);
	*kva = h->kva;
	*bus = cookie.dmac_laddress;
	*hp = h;
	return (0);
fail_mem:
	ddi_dma_mem_free(&h->acch);
fail_handle:
	ddi_dma_free_handle(&h->dmah);
fail:
	kmem_free(h, sizeof (*h));
	return (-ENOMEM);
}

void
lkpi_plat_dma_free_coherent(struct lkpi_plat_dma *h)
{
	(void) ddi_dma_unbind_handle(h->dmah);
	ddi_dma_mem_free(&h->acch);
	ddi_dma_free_handle(&h->dmah);
	kmem_free(h, sizeof (*h));
}

int
lkpi_plat_dma_map(void *dip, u64 mask, void *kva, size_t size, int dir,
    u64 *bus, struct lkpi_plat_dma **hp)
{
	ddi_dma_attr_t attr;
	ddi_dma_cookie_t cookie;
	struct lkpi_plat_dma *h;
	uint_t ccount, flags;

	h = kmem_zalloc(sizeof (*h), KM_SLEEP);
	lkpi_dma_attr(&attr, mask, 1);
	if (ddi_dma_alloc_handle(dip, &attr, DDI_DMA_SLEEP, NULL,
	    &h->dmah) != DDI_SUCCESS) {
		kmem_free(h, sizeof (*h));
		return (-ENOMEM);
	}
	flags = DDI_DMA_STREAMING | (dir == LKPI_DMA_TO_DEVICE ? DDI_DMA_WRITE :
	    (dir == LKPI_DMA_FROM_DEVICE ? DDI_DMA_READ : DDI_DMA_RDWR));
	if (ddi_dma_addr_bind_handle(h->dmah, NULL, kva, size, flags,
	    DDI_DMA_SLEEP, NULL, &cookie, &ccount) != DDI_DMA_MAPPED) {
		ddi_dma_free_handle(&h->dmah);
		kmem_free(h, sizeof (*h));
		return (-EIO);
	}
	if (ccount != 1) {
		/* Not bus-contiguous (no IOMMU and more than one page). */
		(void) ddi_dma_unbind_handle(h->dmah);
		ddi_dma_free_handle(&h->dmah);
		kmem_free(h, sizeof (*h));
		return (-EINVAL);
	}
	*bus = cookie.dmac_laddress;
	*hp = h;
	return (0);
}

/*
 * Separately allocated pages as one DMA object: a B_SHADOW buf whose
 * b_shadow page_t list gives rootnex each page's PFN
 * (ddi_dma_buf_bind_handle -> DMA_OTYP_BUFVADDR with v_priv = pplist;
 * rootnex_get_sgl walks pplist instead of hat_getpfnum(v_addr)). b_addr
 * only supplies the in-page offset of the first page. The page_t of a
 * KPI page is page_numtopp_nolock(pfn): KPI pages are locked kernel
 * memory from contig_alloc. If rootnex had to bounce (an address above
 * the DMA mask), its copy would use b_addr as a contiguous VA, which it
 * is not: the 39-bit i915 mask covers all RAM of the target machines.
 * UNTESTED ON ILLUMOS.
 */
int
lkpi_plat_dma_map_pages(void *dip, u64 mask, void *const *kvas,
    const u64 *phys, size_t npages, unsigned int off, size_t size, int dir,
    u64 *bus, struct lkpi_plat_dma **hp)
{
	ddi_dma_attr_t attr;
	ddi_dma_cookie_t cookie;
	struct lkpi_plat_dma *h;
	uint_t ccount, flags;
	size_t i;

	h = kmem_zalloc(sizeof (*h), KM_SLEEP);
	h->npages = npages;
	h->pplist = kmem_zalloc(npages * sizeof (page_t *), KM_SLEEP);
	for (i = 0; i < npages; i++) {
		h->pplist[i] = page_numtopp_nolock(
		    (pfn_t)(phys[i] >> PAGESHIFT));
		if (h->pplist[i] == NULL)
			goto fail_list;
	}
	h->bp = getrbuf(KM_SLEEP);
	h->bp->b_flags = B_BUSY | B_SHADOW |
	    (dir == LKPI_DMA_FROM_DEVICE ? B_READ : B_WRITE);
	h->bp->b_bcount = size;
	h->bp->b_un.b_addr = (caddr_t)kvas[0] + off;
	h->bp->b_shadow = h->pplist;
	h->bp->b_proc = NULL;
	lkpi_dma_attr(&attr, mask, 1);
	if (ddi_dma_alloc_handle(dip, &attr, DDI_DMA_SLEEP, NULL,
	    &h->dmah) != DDI_SUCCESS)
		goto fail_buf;
	flags = DDI_DMA_STREAMING | (dir == LKPI_DMA_TO_DEVICE ? DDI_DMA_WRITE :
	    (dir == LKPI_DMA_FROM_DEVICE ? DDI_DMA_READ : DDI_DMA_RDWR));
	if (ddi_dma_buf_bind_handle(h->dmah, h->bp, flags, DDI_DMA_SLEEP,
	    NULL, &cookie, &ccount) != DDI_DMA_MAPPED)
		goto fail_handle;
	if (ccount != 1) {
		(void) ddi_dma_unbind_handle(h->dmah);
		goto fail_handle;
	}
	*bus = cookie.dmac_laddress;
	*hp = h;
	return (0);
fail_handle:
	ddi_dma_free_handle(&h->dmah);
fail_buf:
	freerbuf(h->bp);
fail_list:
	kmem_free(h->pplist, npages * sizeof (page_t *));
	kmem_free(h, sizeof (*h));
	return (-EINVAL);
}

void
lkpi_plat_dma_unmap(struct lkpi_plat_dma *h)
{
	(void) ddi_dma_unbind_handle(h->dmah);
	ddi_dma_free_handle(&h->dmah);
	if (h->bp != NULL) {
		freerbuf(h->bp);
		kmem_free(h->pplist, h->npages * sizeof (page_t *));
	}
	kmem_free(h, sizeof (*h));
}

void
lkpi_plat_dma_sync(struct lkpi_plat_dma *h, size_t off, size_t len,
    bool for_device)
{
	(void) ddi_dma_sync(h->dmah, (off_t)off, len,
	    for_device ? DDI_DMA_SYNC_FORDEV : DDI_DMA_SYNC_FORCPU);
}

static kmem_cache_t *lkpi_page_cache;

/* Created on first use; a racing loser destroys its copy. */
static kmem_cache_t *
lkpi_page_cache_get(void)
{
	kmem_cache_t *kc = lkpi_page_cache, *n;

	if (kc != NULL)
		return (kc);
	n = kmem_cache_create("linuxkpi_page", PAGESIZE, PAGESIZE, NULL,
	    NULL, NULL, NULL, NULL, 0);
	if (n == NULL)
		return (NULL);
	kc = NULL;
	if (!__atomic_compare_exchange_n(&lkpi_page_cache, &kc, n, false,
	    __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
		kmem_cache_destroy(n);
		return (kc);
	}
	return (n);
}

/* Called at linuxkpi _fini, after every KPI page was freed. */
void
lkpi_plat_page_cache_fini(void)
{
	if (lkpi_page_cache != NULL) {
		kmem_cache_destroy(lkpi_page_cache);
		lkpi_page_cache = NULL;
	}
}

/*
 * Physically contiguous memory: DMA memory allocated against the root
 * node (no IOMMU there) with one segment and page alignment.
 */
void *
lkpi_plat_contig_alloc(size_t size, bool zero, bool cansleep, void **cookie)
{
	struct lkpi_plat_dma *h;

	/*
	 * Single pages (alloc_page(), the bulk of GEM backing store) come
	 * from a page-sized, page-aligned kmem cache: no DMA handle and no
	 * contig_alloc() search per page (docs/12 M3). Kernel heap memory is
	 * locked and has page_t (page_numtopp_nolock works for DMA binds).
	 * cookie NULL marks this path. Callers that changed the cache
	 * attribute must restore write-back before freeing (as on Linux).
	 */
	if (size == PAGESIZE) {
		kmem_cache_t *kc = lkpi_page_cache_get();
		void *p;

		if (kc == NULL)
			return (NULL);
		p = kmem_cache_alloc(kc, cansleep ? KM_SLEEP : KM_NOSLEEP);
		if (p != NULL && zero)
			bzero(p, PAGESIZE);
		*cookie = NULL;
		return (p);
	}
	ddi_device_acc_attr_t acc;
	ddi_dma_attr_t attr;
	int (*wait)(caddr_t) = cansleep ? DDI_DMA_SLEEP : DDI_DMA_DONTWAIT;

	h = kmem_zalloc(sizeof (*h), cansleep ? KM_SLEEP : KM_NOSLEEP);
	if (h == NULL)
		return (NULL);
	lkpi_dma_attr(&attr, ~0ULL, PAGESIZE);
	lkpi_acc_attr(&acc, LKPI_CACHE_ATTR_WB);
	if (ddi_dma_alloc_handle(ddi_root_node(), &attr, wait, NULL,
	    &h->dmah) != DDI_SUCCESS) {
		kmem_free(h, sizeof (*h));
		return (NULL);
	}
	if (ddi_dma_mem_alloc(h->dmah, size, &acc, DDI_DMA_CONSISTENT, wait,
	    NULL, &h->kva, &h->real_len, &h->acch) != DDI_SUCCESS) {
		ddi_dma_free_handle(&h->dmah);
		kmem_free(h, sizeof (*h));
		return (NULL);
	}
	if (zero)
		bzero(h->kva, size);
	*cookie = h;
	return (h->kva);
}

void
lkpi_plat_contig_free(void *kva, size_t size, void *cookie)
{
	struct lkpi_plat_dma *h = cookie;

	if (h == NULL) {
		kmem_cache_free(lkpi_page_cache, kva);
		return;
	}
	(void) size;
	ddi_dma_mem_free(&h->acch);
	ddi_dma_free_handle(&h->dmah);
	kmem_free(h, sizeof (*h));
}

u64
lkpi_plat_virt_to_phys(const void *kva)
{
	pfn_t pfn = hat_getpfnum(kas.a_hat, (caddr_t)(uintptr_t)kva);

	return (((u64)pfn << PAGESHIFT) | ((uintptr_t)kva & PAGEOFFSET));
}

static uint_t
lkpi_hat_attr(int cache)
{
	switch (cache) {
	case LKPI_CACHE_ATTR_WB:
		return (HAT_STORECACHING_OK);
	case LKPI_CACHE_ATTR_WC:
		return (HAT_MERGING_OK | HAT_PLAT_NOCACHE);
	default:
		return (HAT_STRICTORDER | HAT_PLAT_NOCACHE);
	}
}

void *
lkpi_plat_vmap(const u64 *phys, size_t npages, int cache)
{
	caddr_t va;
	size_t i;

	va = vmem_alloc(heap_arena, ptob(npages), VM_SLEEP);
	for (i = 0; i < npages; i++)
		hat_devload(kas.a_hat, va + ptob(i), PAGESIZE,
		    (pfn_t)(phys[i] >> PAGESHIFT),
		    PROT_READ | PROT_WRITE | HAT_NOSYNC | lkpi_hat_attr(cache),
		    HAT_LOAD_LOCK);
	return (va);
}

void
lkpi_plat_vunmap(void *va, size_t npages)
{
	hat_unload(kas.a_hat, va, ptob(npages), HAT_UNLOAD_UNLOCK);
	vmem_free(heap_arena, va, ptob(npages));
}

/*
 * Change the cache attribute of a kernel mapping (set_memory_*,
 * set_pages_*; docs/12 C2). illumos has kmem_override_cache_attrs() in
 * i86pc ddi_impl.c, but it is static, so the same sequence is done here
 * with the public HAT interface: for each page, keep protections, unload
 * the locked translation and reload it with the new HAT order. Dirty
 * lines are written back first (WB -> UC/WC must not leave stale data
 * behind the new attribute). Only for KPI memory (contig_alloc pages and
 * vmap ranges), which is mapped locked in kas.
 * UNTESTED ON ILLUMOS.
 */
int
lkpi_plat_set_cache(void *kva, size_t len, int cache)
{
	caddr_t va = (caddr_t)((uintptr_t)kva & (uintptr_t)PAGEMASK);
	caddr_t end = (caddr_t)(((uintptr_t)kva + len + PAGEOFFSET) &
	    (uintptr_t)PAGEMASK);
	uint_t attr;
	pfn_t pfn;
	caddr_t p;

	for (p = va; p < end; p += PAGESIZE) {
		if (hat_getattr(kas.a_hat, p, &attr) == (uint_t)-1)
			return (-EINVAL);
	}
	for (p = va; p < end; p += 64)
		__asm__ __volatile__("clflush %0" : "+m" (*(volatile char *)p));
	__asm__ __volatile__("mfence" : : : "memory");
	for (p = va; p < end; p += PAGESIZE) {
		(void) hat_getattr(kas.a_hat, p, &attr);
		pfn = hat_getpfnum(kas.a_hat, p);
		hat_unload(kas.a_hat, p, PAGESIZE, HAT_UNLOAD_UNLOCK);
		hat_devload(kas.a_hat, p, PAGESIZE, pfn,
		    (attr & HAT_PROT_MASK) | HAT_NOSYNC | lkpi_hat_attr(cache),
		    HAT_LOAD_LOCK);
	}
	return (0);
}

unsigned long
lkpi_plat_physmem_pages(void)
{
	return ((unsigned long)physmem);
}

/* Firmware files through krtld (works once the root file system is up). */
int
lkpi_plat_read_file(const char *path, void **bufp, size_t *lenp)
{
	struct _buf *f;
	uint64_t size;
	void *buf;
	int n;

	f = kobj_open_file((char *)path);
	if (f == (struct _buf *)-1)
		return (-ENOENT);
	if (kobj_get_filesize(f, &size) != 0 || size > (64ULL << 20)) {
		kobj_close_file(f);
		return (-EIO);
	}
	buf = kmem_alloc(size > 0 ? (size_t)size : 1, KM_SLEEP);
	n = size > 0 ? kobj_read_file(f, buf, (unsigned)size, 0) : 0;
	kobj_close_file(f);
	if (n < 0 || (uint64_t)n != size) {
		kmem_free(buf, size > 0 ? (size_t)size : 1);
		return (-EIO);
	}
	*bufp = buf;
	*lenp = (size_t)size;
	return (0);
}

/* DMI strings from the kernel SMBIOS snapshot (ksmbios), cached. */
#define	LKPI_DMI_MAX	32
static char lkpi_dmi_buf[LKPI_DMI_MAX][64];
static bool lkpi_dmi_have[LKPI_DMI_MAX];
static kmutex_t lkpi_dmi_lock;
static bool lkpi_dmi_loaded;

static void
lkpi_dmi_set(int field, const char *s)
{
	if (s == NULL || *s == '\0' || field < 0 || field >= LKPI_DMI_MAX)
		return;
	(void) strlcpy(lkpi_dmi_buf[field], s, sizeof (lkpi_dmi_buf[field]));
	lkpi_dmi_have[field] = true;
}

/* Field numbers follow Linux enum dmi_field (<linux/dmi.h>). */
static void
lkpi_dmi_load(void)
{
	smbios_bios_t bios;
	smbios_system_t sys;
	smbios_info_t info;
	smbios_struct_t st;
	id_t id;

	if (ksmbios == NULL)
		return;
	if (smbios_info_bios(ksmbios, &bios) != -1) {
		lkpi_dmi_set(1, bios.smbb_vendor);
		lkpi_dmi_set(2, bios.smbb_version);
		lkpi_dmi_set(3, bios.smbb_reldate);
	}
	id = smbios_info_system(ksmbios, &sys);
	if (id != -1 && smbios_info_common(ksmbios, id, &info) != -1) {
		lkpi_dmi_set(6, info.smbi_manufacturer);
		lkpi_dmi_set(7, info.smbi_product);
		lkpi_dmi_set(8, info.smbi_version);
		lkpi_dmi_set(9, info.smbi_serial);
	}
	if (smbios_lookup_type(ksmbios, SMB_TYPE_BASEBOARD, &st) != -1 &&
	    smbios_info_common(ksmbios, st.smbstr_id, &info) != -1) {
		lkpi_dmi_set(13, info.smbi_manufacturer);
		lkpi_dmi_set(14, info.smbi_product);
		lkpi_dmi_set(15, info.smbi_version);
	}
	if (smbios_lookup_type(ksmbios, SMB_TYPE_CHASSIS, &st) != -1 &&
	    smbios_info_common(ksmbios, st.smbstr_id, &info) != -1)
		lkpi_dmi_set(18, info.smbi_manufacturer);
}

const char *
lkpi_plat_dmi_string(int field)
{
	if (field < 0 || field >= LKPI_DMI_MAX)
		return (NULL);
	mutex_enter(&lkpi_dmi_lock);
	if (!lkpi_dmi_loaded) {
		lkpi_dmi_load();
		lkpi_dmi_loaded = true;
	}
	mutex_exit(&lkpi_dmi_lock);
	return (lkpi_dmi_have[field] ? lkpi_dmi_buf[field] : NULL);
}

/* ACPI through ACPICA (the illumos acpica module). */
int
lkpi_plat_acpi_get_handle(void *parent, const char *path, void **h)
{
	ACPI_HANDLE ah;

	if (AcpiGetHandle(parent, (ACPI_STRING)(uintptr_t)path, &ah) != AE_OK)
		return (-ENODEV);
	*h = ah;
	return (0);
}

int
lkpi_plat_acpi_evaluate(void *h, const char *method, void *args, int nargs,
    void **result)
{
	ACPI_OBJECT_LIST list;
	ACPI_BUFFER buf;
	ACPI_STATUS st;

	list.Count = (UINT32)nargs;
	list.Pointer = args;
	buf.Length = ACPI_ALLOCATE_BUFFER;
	buf.Pointer = NULL;
	st = AcpiEvaluateObject(h, (ACPI_STRING)(uintptr_t)method,
	    nargs > 0 ? &list : NULL, &buf);
	if (st == AE_NOT_FOUND)
		return (-ENOENT);
	if (st != AE_OK)
		return (-EIO);
	*result = buf.Pointer;
	return (0);
}

void
lkpi_plat_acpi_free(void *obj)
{
	if (obj != NULL)
		AcpiOsFree(obj);
}

void *
lkpi_plat_acpi_dev_handle(void *dip)
{
	ACPI_HANDLE h;

	if (acpica_get_handle(dip, &h) != AE_OK)
		return (NULL);
	return (h);
}

/*
 * uevent as sysevent: class EC_linuxkpi, subclass ESC_uevent, string
 * attributes ACTION, DEVPATH and each KEY=VALUE of the environment.
 */
void
lkpi_plat_uevent(void *dip, const char *action, const char *devpath,
    char *const *envp, int nenv)
{
	nvlist_t *nvl;
	sysevent_id_t eid;
	char key[64];
	int i;

	if (dip == NULL)
		return;
	if (nvlist_alloc(&nvl, NV_UNIQUE_NAME_TYPE, KM_SLEEP) != 0)
		return;
	(void) nvlist_add_string(nvl, "ACTION", (char *)action);
	(void) nvlist_add_string(nvl, "DEVPATH", (char *)devpath);
	for (i = 0; i < nenv; i++) {
		const char *eq = strchr(envp[i], '=');
		size_t kl;

		if (eq == NULL)
			continue;
		kl = (size_t)(eq - envp[i]);
		if (kl >= sizeof (key))
			kl = sizeof (key) - 1;
		memcpy(key, envp[i], kl);
		key[kl] = '\0';
		(void) nvlist_add_string(nvl, key, (char *)(uintptr_t)(eq + 1));
	}
	(void) ddi_log_sysevent(dip, DDI_VENDOR_SUNW, "EC_linuxkpi",
	    "ESC_uevent", nvl, &eid, DDI_SLEEP);
	nvlist_free(nvl);
}

u64
lkpi_plat_rdmsr(u32 msr)
{
	return (rdmsr(msr));
}

void
lkpi_plat_wrmsr(u32 msr, u64 val)
{
	wrmsr(msr, val);
}

/* Write back and invalidate this CPU's caches (cross-call handler). */
static int
lkpi_wbinvd_xc(xc_arg_t a1, xc_arg_t a2, xc_arg_t a3)
{
	(void) a1;
	(void) a2;
	(void) a3;
	__asm__ __volatile__("wbinvd" : : : "memory");
	return (0);
}

/*
 * WBINVD on every CPU (docs/12 C1): a cross call to all CPUs, which
 * also runs the handler on the calling CPU, and returns once all have
 * finished. Preemption is disabled so the caller cannot migrate halfway.
 */
void
lkpi_plat_wbinvd(void)
{
	cpuset_t set;

	kpreempt_disable();
	CPUSET_ALL(set);
	xc_call(0, 0, 0, CPUSET2BV(set), lkpi_wbinvd_xc);
	kpreempt_enable();
}

#endif /* LINUXKPI_ILLUMOS_KERNEL */

/*
 * IOMMU (illumos immu, part of drv/rootnex: MODDEPS drv/rootnex). Display
 * devices always get a unity (1:1) DVMA domain, so DMA addresses equal
 * physical ones, but the DMAR unit still translates through page tables;
 * Gen9 i915 needs its VT-d workarounds then (intel_vtd_active()).
 * Translation is on when immu runs with DVMA enabled and graphics DVMA
 * not excluded ("immu-gfxdvma-enable", "immu-dvma-enable", "immu-enable"
 * boot properties or /etc/system). A graphics-only DMAR unit is not
 * started when immu_gfxdvma_enable is false; a GPU behind a shared unit
 * would still be translated then (not detected: use the override).
 */
extern boolean_t immu_running;
extern boolean_t immu_dvma_enable;
extern boolean_t immu_gfxdvma_enable;

int linuxkpi_iommu_mapped = -1;

bool
lkpi_plat_dev_iommu_mapped(void *dip)
{
	(void) dip;
	if (linuxkpi_iommu_mapped >= 0)
		return (linuxkpi_iommu_mapped != 0);
	return (immu_running && immu_dvma_enable && immu_gfxdvma_enable);
}
