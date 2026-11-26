/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * Random numbers (see <linux/random.h>) from the platform source:
 * random_get_pseudo_bytes(9F) on illumos, /dev/urandom in userland.
 */

#include <linux/kernel.h>
#include <linux/random.h>

void
get_random_bytes(void *buf, size_t len)
{
	lkpi_plat_random(buf, len);
}

u32
get_random_u32(void)
{
	u32 v;

	lkpi_plat_random(&v, sizeof (v));
	return (v);
}

u64
get_random_u64(void)
{
	u64 v;

	lkpi_plat_random(&v, sizeof (v));
	return (v);
}
