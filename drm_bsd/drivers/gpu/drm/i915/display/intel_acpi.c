/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 *
 * i915 ACPI helpers for psi915: clean-room replacement for Linux i915 display/intel_acpi.c
 * (GPL-2.0).  Implements the interface of the MIT intel_acpi.h with the behaviour psi915 needs on
 * Kaby Lake-R laptops.  Not derived from the Linux file.
 *
 * What each upstream function is for, and what psi915 does:
 *  - intel_register_dsm_handler / intel_unregister_dsm_handler: hook the Intel graphics _DSM for
 *    vga_switcheroo on muxed hybrid-graphics machines.  vga_switcheroo is not supported
 *    (CONFIG_VGA_SWITCHEROO off), so these do nothing.
 *  - intel_dsm_get_bios_data_funcs_supported: query the same _DSM so that some firmware enables
 *    its graphics-related BIOS data functions.  Not needed for KBL-R bring-up; no-op, to be revisited
 *    with the KPI's acpi_evaluate_dsm() if a machine depends on it.
 *  - intel_acpi_device_id_update: write the connectors' ACPI display IDs into the OpRegion DIDL for
 *    ACPI video (brightness hotkeys).  CONFIG_ACPI_VIDEO is off; no-op.
 *  - intel_acpi_assign_connector_fwnodes: attach ACPI firmware nodes to connectors (privacy screens
 *    found through ACPI).  No-op: connectors keep no fwnode, so no ACPI privacy screen is found.
 *  - intel_acpi_video_register: register with the ACPI video driver.  CONFIG_ACPI_VIDEO off; no-op.
 * Each no-op is a reduction of upstream behaviour, recorded in docs/08 section 5.
 */

#include <linux/types.h>

#include "intel_acpi.h"

struct intel_display;

void
intel_register_dsm_handler(void)
{
}

void
intel_unregister_dsm_handler(void)
{
}

void
intel_dsm_get_bios_data_funcs_supported(struct intel_display *display)
{
}

void
intel_acpi_device_id_update(struct intel_display *display)
{
}

void
intel_acpi_assign_connector_fwnodes(struct intel_display *display)
{
}

void
intel_acpi_video_register(struct intel_display *display)
{
}
