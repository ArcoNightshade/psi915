/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Platform pieces Linux provides outside the i915 driver:
 *
 *   intel_graphics_stolen_res  The graphics stolen memory range. Linux
 *                              finds it in x86 early quirks; here the PCI
 *                              probe path fills it (lkpi_intel_gfx_quirk)
 *                              from the integrated GPU's config space
 *                              before the driver's probe runs. Gen9 and
 *                              later register layout (BDSM at 0x5c, GMS in
 *                              GGC bits 15:8 at 0x50), documented in the
 *                              Intel graphics PRMs; the driver_aliases of
 *                              this port are Gen9 only.
 *   intel_gmch_*               The intel-gtt (AGP GMCH) interface for
 *                              Gen2-5 GTTs. Not supported: probe fails, so
 *                              i915 refuses those GPUs; the other entry
 *                              points are never reached.
 *   register_vmap_purge_notifier  Notifiers called when vmalloc space is
 *                              purged. illumos has no such event, so they
 *                              are kept but never called.
 */

#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/ioport.h>
#include <linux/notifier.h>
#include <linux/vmalloc.h>
#include <linuxkpi/intel_gfx.h>

struct resource intel_graphics_stolen_res = DEFINE_RES_MEM(0, 0);

/* Gen9+: GMS < 0xf0 counts 32 MiB units, 0xf0..0xfe 4 MiB units from 4 MiB. */
bool
lkpi_intel_gen9_stolen(u16 ggc, u32 bdsm, struct resource *res)
{
	unsigned int gms = (ggc >> 8) & 0xff;
	u64 size, base = bdsm & 0xfff00000U;

	if (gms < 0xf0)
		size = (u64)gms * 32 * 1024 * 1024;
	else
		size = (u64)(gms - 0xf0 + 1) * 4 * 1024 * 1024;
	if (size == 0 || base == 0)
		return (false);
	res->start = base;
	res->end = base + size - 1;
	res->flags = IORESOURCE_MEM;
	res->name = "Graphics Stolen Memory";
	return (true);
}

/* Called for every device before its driver probes (linux_pci.c). */
void
lkpi_intel_gfx_quirk(struct pci_dev *pdev)
{
	u16 ggc;
	u32 bdsm;

	if (pdev->vendor != PCI_VENDOR_ID_INTEL || (pdev->class >> 16) != 0x03 ||
	    pdev->devfn != PCI_DEVFN(2, 0) || pdev->bus == NULL ||
	    pdev->bus->number != 0 || resource_size(&intel_graphics_stolen_res) != 0)
		return;
	if (pci_read_config_word(pdev, 0x50, &ggc) != 0 ||
	    pci_read_config_dword(pdev, 0x5c, &bdsm) != 0)
		return;
	(void) lkpi_intel_gen9_stolen(ggc, bdsm, &intel_graphics_stolen_res);
}

/* intel-gtt (Gen2-5 GMCH): not supported. */
int
intel_gmch_probe(struct pci_dev *bridge_pdev, struct pci_dev *gpu_pdev,
    struct agp_bridge_data *bridge)
{
	(void) bridge_pdev;
	(void) gpu_pdev;
	(void) bridge;
	return (0);		/* 0 = no GMCH found (i915: -EIO) */
}

void
intel_gmch_remove(void)
{
}

void
intel_gmch_gtt_get(u64 *gtt_total, phys_addr_t *mappable_base,
    resource_size_t *mappable_end)
{
	*gtt_total = 0;
	*mappable_base = 0;
	*mappable_end = 0;
}

bool
intel_gmch_enable_gtt(void)
{
	return (false);
}

void
intel_gmch_gtt_flush(void)
{
}

void
intel_gmch_gtt_insert_page(dma_addr_t addr, unsigned int pg,
    unsigned int flags)
{
	(void) addr;
	(void) pg;
	(void) flags;
}

void
intel_gmch_gtt_insert_sg_entries(struct sg_table *st, unsigned int pg_start,
    unsigned int flags)
{
	(void) st;
	(void) pg_start;
	(void) flags;
}

void
intel_gmch_gtt_clear_range(unsigned int first_entry, unsigned int num_entries)
{
	(void) first_entry;
	(void) num_entries;
}

/* vmap purge notifiers: registered, never called (no such illumos event). */
static BLOCKING_NOTIFIER_HEAD(lkpi_vmap_purge_chain);

int
register_vmap_purge_notifier(struct notifier_block *nb)
{
	return (blocking_notifier_chain_register(&lkpi_vmap_purge_chain, nb));
}

int
unregister_vmap_purge_notifier(struct notifier_block *nb)
{
	return (blocking_notifier_chain_unregister(&lkpi_vmap_purge_chain, nb));
}
