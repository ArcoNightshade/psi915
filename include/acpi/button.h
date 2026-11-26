/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. No ACPI button driver: the lid state is unknown.
 */
#ifndef _LINUXKPI_INCLUDE_ACPI_BUTTON_H_
#define	_LINUXKPI_INCLUDE_ACPI_BUTTON_H_

#include <linux/errno.h>
#define	acpi_lid_open()	(-ENODEV)

#endif /* _LINUXKPI_INCLUDE_ACPI_BUTTON_H_ */
