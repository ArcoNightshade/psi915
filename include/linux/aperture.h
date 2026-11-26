/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Firmware framebuffer hand-off: prototypes of the imported
 * drivers/video/aperture.c. On illumos the boot console (vgatext / tem
 * over the VBE/EFI framebuffer) is taken over by the DRM console glue
 * (P4); there are no Linux firmware-framebuffer drivers to remove.
 */
#ifndef _LINUXKPI_LINUX_APERTURE_H_
#define	_LINUXKPI_LINUX_APERTURE_H_

#include <linux/types.h>

struct pci_dev;
struct platform_device;

/*
 * Implemented by the imported drivers/video/aperture.c (MIT), which
 * hands firmware framebuffers over to the native driver.
 */
int devm_aperture_acquire_for_platform_device(struct platform_device *pdev,
    resource_size_t base, resource_size_t size);
int aperture_remove_conflicting_devices(resource_size_t base,
    resource_size_t size, const char *name);
int __aperture_remove_legacy_vga_devices(struct pci_dev *pdev);
int aperture_remove_conflicting_pci_devices(struct pci_dev *pdev,
    const char *name);

static inline int
aperture_remove_all_conflicting_devices(const char *name)
{
	return (aperture_remove_conflicting_devices(0, (resource_size_t)-1,
	    name));
}

#endif /* _LINUXKPI_LINUX_APERTURE_H_ */
