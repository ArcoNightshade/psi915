/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. No hybrid-graphics switching on illumos.
 */
#ifndef _LINUXKPI_LINUX_VGA_SWITCHEROO_H_
#define	_LINUXKPI_LINUX_VGA_SWITCHEROO_H_

#include <linux/types.h>

struct pci_dev;
struct vga_switcheroo_client_ops;
enum vga_switcheroo_state {
	VGA_SWITCHEROO_OFF,
	VGA_SWITCHEROO_ON,
	VGA_SWITCHEROO_NOT_FOUND
};
#define	vga_switcheroo_register_client(p, o, d)	((void)(p), 0)
#define	vga_switcheroo_unregister_client(p)	do { (void)(p); } while (0)
#define	vga_switcheroo_process_delayed_switch()	do { } while (0)
#define	vga_switcheroo_handler_flags()		(0)
#define	vga_switcheroo_client_fb_set(p, i)	do { } while (0)

#endif /* _LINUXKPI_LINUX_VGA_SWITCHEROO_H_ */
