/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Word part extraction.
 */
#ifndef _LINUXKPI_LINUX_WORDPART_H_
#define	_LINUXKPI_LINUX_WORDPART_H_

#include <linux/kernel.h>

#ifndef upper_16_bits
#define	upper_16_bits(n)	((u16)((n) >> 16))
#define	lower_16_bits(n)	((u16)((n) & 0xffff))
#endif
#define	REPEAT_BYTE(x)		((~0UL / 0xff) * (x))

#endif /* _LINUXKPI_LINUX_WORDPART_H_ */
