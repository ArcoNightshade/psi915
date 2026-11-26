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
#include <linux/fb.h>

struct pci_dev;
#define	VGA_SWITCHEROO_CAN_SWITCH_DDC	(1 << 0)
#define	VGA_SWITCHEROO_NEEDS_EDP_CONFIG	(1 << 1)

enum vga_switcheroo_state {
	VGA_SWITCHEROO_OFF,
	VGA_SWITCHEROO_ON,
	VGA_SWITCHEROO_NOT_FOUND
};

/* No GPU switching on illumos: clients register but are never called. */
struct vga_switcheroo_client_ops {
	void	(*set_gpu_state)(struct pci_dev *pdev,
		    enum vga_switcheroo_state state);
	void	(*reprobe)(struct pci_dev *pdev);
	bool	(*can_switch)(struct pci_dev *pdev);
	void	(*gpu_bound)(struct pci_dev *pdev, int id);
};
#define	vga_switcheroo_register_client(p, o, d)	((void)(p), 0)
#define	vga_switcheroo_unregister_client(p)	do { (void)(p); } while (0)
#define	vga_switcheroo_process_delayed_switch()	do { } while (0)
#define	vga_switcheroo_handler_flags()		(0)
#define	vga_switcheroo_lock_ddc(p)		((void)(p), -ENODEV)
#define	vga_switcheroo_unlock_ddc(p)		((void)(p), -ENODEV)
#define	vga_switcheroo_client_probe_defer(p)	((void)(p), false)
#define	vga_switcheroo_client_fb_set(p, i)	do { } while (0)

#endif /* _LINUXKPI_LINUX_VGA_SWITCHEROO_H_ */
