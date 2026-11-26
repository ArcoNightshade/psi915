/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Firmware framebuffer hand-off. On illumos the boot console (vgatext /
 * tem over the VBE/EFI framebuffer) is taken over by the DRM console glue
 * (P4); removing conflicting drivers is a no-op that succeeds.
 */
#ifndef _LINUXKPI_LINUX_APERTURE_H_
#define	_LINUXKPI_LINUX_APERTURE_H_

#include <linux/types.h>

struct pci_dev;
#define	aperture_remove_conflicting_pci_devices(p, n)	((void)(p), 0)
#define	aperture_remove_all_conflicting_devices(n)	(0)
#define	aperture_remove_conflicting_devices(b, s, n)	(0)

#endif /* _LINUXKPI_LINUX_APERTURE_H_ */
