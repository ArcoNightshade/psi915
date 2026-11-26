/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/* capable() (see <linux/capability.h>). Clean-room. */

#include <linux/kernel.h>
#include <linux/capability.h>

bool
lkpi_capable(int cap)
{
	(void) cap;
	return (lkpi_plat_privileged());
}
