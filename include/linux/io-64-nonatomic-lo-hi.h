/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. 64-bit MMIO as two 32-bit accesses, low word first. amd64 has
 * native readq/writeq (<linux/io.h>); these are for code that asks for the
 * split explicitly.
 */
#ifndef _LINUXKPI_INCLUDE_LINUX_IO_64_NONATOMIC_LO_HI_H_
#define	_LINUXKPI_INCLUDE_LINUX_IO_64_NONATOMIC_LO_HI_H_

#include <linux/io.h>

static inline u64
lo_hi_readq_split(const volatile void __iomem *a)
{
	u32 lo = readl(a), hi = readl((const volatile char __iomem *)a + 4);

	return (((u64)hi << 32) | lo);
}

static inline void
lo_hi_writeq_split(u64 v, volatile void __iomem *a)
{
	writel((u32)v, a);
	writel((u32)(v >> 32), (volatile char __iomem *)a + 4);
}

#endif /* _LINUXKPI_INCLUDE_LINUX_IO_64_NONATOMIC_LO_HI_H_ */
