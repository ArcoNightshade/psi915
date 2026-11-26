/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation of the Linux DMI quirk-matching API
 * (src/linux_acpi.c). Strings come from the illumos kernel SMBIOS
 * snapshot (ksmbios): BIOS vendor/version/date, system manufacturer/
 * product/version/serial, board vendor/name/version, chassis vendor.
 * Fields not listed read as absent, so entries matching them never
 * match. A match is a substring match unless exact_match is set.
 */
#ifndef _LINUXKPI_LINUX_DMI_H_
#define	_LINUXKPI_LINUX_DMI_H_

#include <linux/types.h>
#include <linux/compiler.h>
#include <linux/mod_devicetable.h>

enum dmi_field {
	DMI_NONE,
	DMI_BIOS_VENDOR,
	DMI_BIOS_VERSION,
	DMI_BIOS_DATE,
	DMI_BIOS_RELEASE,
	DMI_EC_FIRMWARE_RELEASE,
	DMI_SYS_VENDOR,
	DMI_PRODUCT_NAME,
	DMI_PRODUCT_VERSION,
	DMI_PRODUCT_SERIAL,
	DMI_PRODUCT_UUID,
	DMI_PRODUCT_SKU,
	DMI_PRODUCT_FAMILY,
	DMI_BOARD_VENDOR,
	DMI_BOARD_NAME,
	DMI_BOARD_VERSION,
	DMI_BOARD_SERIAL,
	DMI_BOARD_ASSET_TAG,
	DMI_CHASSIS_VENDOR,
	DMI_CHASSIS_TYPE,
	DMI_CHASSIS_VERSION,
	DMI_CHASSIS_SERIAL,
	DMI_CHASSIS_ASSET_TAG,
	DMI_STRING_MAX,
	DMI_OEM_STRING
};

int dmi_check_system(const struct dmi_system_id *list);
const struct dmi_system_id *dmi_first_match(const struct dmi_system_id *list);
const char *dmi_get_system_info(int field);
bool dmi_match(enum dmi_field f, const char *str);
#define	dmi_name_in_vendors(s)	(dmi_match(DMI_SYS_VENDOR, s))

#endif /* _LINUXKPI_LINUX_DMI_H_ */
