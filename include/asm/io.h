/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. See <linux/io.h>. (illumos installs no <asm/io.h>, so this
 * name does not shadow a system header; see docs/03.)
 */
#ifndef _LINUXKPI_ASM_IO_H_
#define	_LINUXKPI_ASM_IO_H_

#include <linux/io.h>

/*
 * Port I/O (Linux argument order: value, port). Legacy VGA registers
 * only. Macros, so an illumos outb(port, value) prototype seen earlier
 * keeps its own meaning.
 */
static inline void
lkpi_outb(u8 v, u16 port)
{
	__asm__ __volatile__("outb %b0, %w1" : : "a" (v), "Nd" (port));
}

static inline u8
lkpi_inb(u16 port)
{
	u8 v;

	__asm__ __volatile__("inb %w1, %b0" : "=a" (v) : "Nd" (port));
	return (v);
}
#define	outb(v, p)	lkpi_outb((v), (p))
#define	inb(p)		lkpi_inb(p)

#endif /* _LINUXKPI_ASM_IO_H_ */
