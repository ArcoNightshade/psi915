/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Device-match tables (DMI; PCI ids are in <linux/pci.h>).
 */
#ifndef _LINUXKPI_LINUX_MOD_DEVICETABLE_H_
#define	_LINUXKPI_LINUX_MOD_DEVICETABLE_H_

#include <linux/types.h>

struct dmi_strmatch {
	unsigned char	slot:7;
	unsigned char	exact_match:1;
	char		substr[79];
};

struct dmi_system_id {
	int			(*callback)(const struct dmi_system_id *);
	const char		*ident;
	struct dmi_strmatch	matches[4];
	void			*driver_data;
};

#define	DMI_MATCH(a, b)		{ .slot = (a), .substr = (b) }
#define	DMI_EXACT_MATCH(a, b)	{ .slot = (a), .substr = (b), .exact_match = 1 }

struct acpi_device_id {
	char		id[16];
	kernel_ulong_t	driver_data;
	u32		cls;
	u32		cls_msk;
};

#endif /* _LINUXKPI_LINUX_MOD_DEVICETABLE_H_ */
