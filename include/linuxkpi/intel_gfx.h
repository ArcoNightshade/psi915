/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. KPI side of Intel integrated graphics platform support
 * (src/linux_intel_gfx.c): the stolen-memory quirk and the intel-gtt
 * and vmap-purge entry points. The declarations of intel_gmch_* that
 * i915 sees are the imported <drm/intel/intel-gtt.h>.
 */
#ifndef _LINUXKPI_INTEL_GFX_H_
#define	_LINUXKPI_INTEL_GFX_H_

#include <linux/types.h>

struct pci_dev;
struct resource;
struct sg_table;
struct agp_bridge_data;
struct notifier_block;

extern struct resource intel_graphics_stolen_res;
bool lkpi_intel_gen9_stolen(u16 ggc, u32 bdsm, struct resource *res);
void lkpi_intel_gfx_quirk(struct pci_dev *pdev);

int intel_gmch_probe(struct pci_dev *bridge_pdev, struct pci_dev *gpu_pdev,
    struct agp_bridge_data *bridge);
void intel_gmch_remove(void);
void intel_gmch_gtt_get(u64 *gtt_total, phys_addr_t *mappable_base,
    resource_size_t *mappable_end);
bool intel_gmch_enable_gtt(void);
void intel_gmch_gtt_flush(void);
void intel_gmch_gtt_insert_page(dma_addr_t addr, unsigned int pg,
    unsigned int flags);
void intel_gmch_gtt_insert_sg_entries(struct sg_table *st,
    unsigned int pg_start, unsigned int flags);
void intel_gmch_gtt_clear_range(unsigned int first_entry,
    unsigned int num_entries);

#endif /* _LINUXKPI_INTEL_GFX_H_ */
