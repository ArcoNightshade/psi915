/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * Internal: hardware backend of the LinuxKPI (P2). Not a Linux header.
 *
 * The Linux-facing code (src/linux_pci.c, linux_irq.c, linux_dma.c,
 * linux_page.c, linux_io.c, linux_firmware.c, linux_acpi.c, ...) is
 * environment-neutral and reaches devices only through these functions:
 *
 *   src/plat_illumos_dev.c   illumos DDI: pci_config_*(9F),
 *                            ddi_regs_map_setup(9F), ddi_intr_*(9F),
 *                            ddi_dma_*(9F), gfx_private kernel mappings,
 *                            kobj file reads, ksmbios, ACPICA, sysevents.
 *                            UNTESTED ON ILLUMOS (stub-header compile only).
 *   src/plat_userland_dev.c  fake devices for the unit tests: config space
 *                            arrays, BARs backed by memory, interrupts
 *                            raised by test code on a thread, an IOMMU-like
 *                            bus address translation for DMA, files read
 *                            from a directory.
 *
 * `dip` is an illumos dev_info_t * (or the fake device) and is opaque
 * here. Functions return 0 or a negative Linux errno unless noted.
 */
#ifndef _LINUXKPI_PLAT_DEV_H_
#define	_LINUXKPI_PLAT_DEV_H_

#include <linuxkpi/platform.h>
#include <linux/types.h>

/* Cache attributes for mappings. */
#define	LKPI_CACHE_ATTR_UC	0	/* strictly ordered, uncached */
#define	LKPI_CACHE_ATTR_WC	1	/* write-combining (PAT) */
#define	LKPI_CACHE_ATTR_WB	2	/* cached */

/* PCI identity and resources of one function. */
#define	LKPI_PCI_NUM_BARS	6
#define	LKPI_BAR_MEM		0x1
#define	LKPI_BAR_IO		0x2
#define	LKPI_BAR_64		0x4
#define	LKPI_BAR_PREFETCH	0x8

struct lkpi_plat_pci_info {
	u16	vendor, device, subvendor, subdevice;
	u32	class;		/* base << 16 | sub << 8 | prog-if */
	u8	revision;
	u16	domain;
	u8	bus;
	u8	devfn;
	struct {
		u64	start;	/* CPU physical address (0: unassigned) */
		u64	len;
		u32	flags;	/* LKPI_BAR_* */
		int	rnumber; /* "reg" index for ddi_regs_map_setup, -1 */
	} bar[LKPI_PCI_NUM_BARS];
	u64	rom_start, rom_len;
};

int lkpi_plat_pci_info(void *dip, struct lkpi_plat_pci_info *info);
/* Config space handle (pci_config_setup(9F)). */
int lkpi_plat_pci_cfg_open(void *dip, void **cfgh);
void lkpi_plat_pci_cfg_close(void *cfgh);
u32 lkpi_plat_pci_cfg_read(void *cfgh, int where, int size);
void lkpi_plat_pci_cfg_write(void *cfgh, int where, int size, u32 val);
/*
 * Visit every PCI function in the device tree (any driver binding);
 * stop when fn returns non-zero. The dip is held during the callback.
 */
void lkpi_plat_pci_walk(int (*fn)(void *dip, void *arg), void *arg);
void lkpi_plat_dev_hold(void *dip);
void lkpi_plat_dev_rele(void *dip);
void *lkpi_plat_dev_parent(void *dip);
const char *lkpi_plat_dev_name(void *dip);	/* e.g. "i915#0" */

/* MMIO: map part of a BAR ("reg" entry rnumber); *handle for unmap. */
void *lkpi_plat_map_regs(void *dip, int rnumber, u64 off, u64 len,
    int cache, void **handle);
void lkpi_plat_unmap_regs(void **handle);
/* Map any physical range (gfx_private gfxp_map_kernel_space). */
void *lkpi_plat_map_phys(u64 phys, size_t len, int cache);
void lkpi_plat_unmap_phys(void *va, size_t len);

/* Interrupts. */
#define	LKPI_INTR_INTX		0x1
#define	LKPI_INTR_MSI		0x2
#define	LKPI_INTR_MSIX		0x4

struct lkpi_plat_intr;
/* Allocate between min and max vectors of one of the types. */
int lkpi_plat_intr_alloc(void *dip, unsigned int types, int min, int max,
    struct lkpi_plat_intr **ip, unsigned int *type, int *count);
void lkpi_plat_intr_free(struct lkpi_plat_intr *ip);
/* fn returns true if the interrupt was claimed. */
int lkpi_plat_intr_add(struct lkpi_plat_intr *ip, int idx,
    bool (*fn)(void *arg), void *arg);
void lkpi_plat_intr_remove(struct lkpi_plat_intr *ip, int idx);
int lkpi_plat_intr_enable(struct lkpi_plat_intr *ip, int idx);
int lkpi_plat_intr_disable(struct lkpi_plat_intr *ip, int idx);
/* Mutex iblock cookie (DDI_INTR_PRI(pri)) for locks the handler takes. */
void *lkpi_plat_intr_iblock(struct lkpi_plat_intr *ip, int idx);
/* True if the vector runs above LOCK_LEVEL (not supported by the KPI). */
bool lkpi_plat_intr_hilevel(struct lkpi_plat_intr *ip, int idx);

/*
 * DMA. Every binding has exactly one segment (dma_attr_sgllen = 1), so
 * a buffer that is not contiguous in bus space fails to bind. Bus
 * addresses come from the DMA cookie (IOMMU-translated if one is on).
 */
#define	LKPI_DMA_TO_DEVICE	1
#define	LKPI_DMA_FROM_DEVICE	2
#define	LKPI_DMA_BIDIR		3

struct lkpi_plat_dma;
int lkpi_plat_dma_alloc_coherent(void *dip, u64 mask, size_t size,
    void **kva, u64 *bus, struct lkpi_plat_dma **h);
void lkpi_plat_dma_free_coherent(struct lkpi_plat_dma *h);
int lkpi_plat_dma_map(void *dip, u64 mask, void *kva, size_t size, int dir,
    u64 *bus, struct lkpi_plat_dma **h);
void lkpi_plat_dma_unmap(struct lkpi_plat_dma *h);
void lkpi_plat_dma_sync(struct lkpi_plat_dma *h, size_t off, size_t len,
    bool for_device);

/* Physically contiguous, page-aligned kernel memory (struct page). */
void *lkpi_plat_contig_alloc(size_t size, bool zero, bool cansleep,
    void **cookie);
void lkpi_plat_contig_free(void *kva, size_t size, void *cookie);
u64 lkpi_plat_virt_to_phys(const void *kva);
/* Map pages (by physical address) into one virtually contiguous range. */
void *lkpi_plat_vmap(const u64 *phys, size_t npages, int cache);
void lkpi_plat_vunmap(void *va, size_t npages);
int lkpi_plat_set_cache(void *kva, size_t len, int cache);

/* Files (firmware): whole file into lkpi_plat_alloc() memory. */
int lkpi_plat_read_file(const char *path, void **buf, size_t *len);

/* DMI/SMBIOS strings, by LKPI_DMI_* index; NULL if unknown. */
const char *lkpi_plat_dmi_string(int field);

/* ACPI (ACPICA). Handles and objects are ACPICA's. */
int lkpi_plat_acpi_get_handle(void *parent, const char *path, void **h);
/* args: ACPI_OBJECT array; *result: ACPICA-allocated, free with ACPI_FREE. */
int lkpi_plat_acpi_evaluate(void *h, const char *method, void *args,
    int nargs, void **result);
void lkpi_plat_acpi_free(void *obj);
void *lkpi_plat_acpi_dev_handle(void *dip);

/* uevent -> sysevent. */
void lkpi_plat_uevent(void *dip, const char *action, const char *devpath,
    char *const *envp, int nenv);

/* CPU */
u64 lkpi_plat_rdmsr(u32 msr);
void lkpi_plat_wrmsr(u32 msr, u64 val);
void lkpi_plat_wbinvd(void);

#endif /* _LINUXKPI_PLAT_DEV_H_ */
