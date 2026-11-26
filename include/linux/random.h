/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room. Random numbers: random_get_pseudo_bytes(9F) on illumos, getrandom-like
 * system source in userland (src/linux_random.c).
 */
#ifndef _LINUXKPI_INCLUDE_LINUX_RANDOM_H_
#define	_LINUXKPI_INCLUDE_LINUX_RANDOM_H_

#include <linux/types.h>

void get_random_bytes(void *buf, size_t len);
u32 get_random_u32(void);
u64 get_random_u64(void);

static inline u32 get_random_u32_below(u32 ceil) { return (ceil == 0 ? 0 : (u32)(((u64)get_random_u32() * ceil) >> 32)); }
static inline u32 get_random_u32_inclusive(u32 lo, u32 hi) { return (lo + get_random_u32_below(hi - lo + 1)); }
static inline u8 get_random_u8(void) { return ((u8)get_random_u32()); }
static inline u16 get_random_u16(void) { return ((u16)get_random_u32()); }
static inline unsigned long get_random_long(void) { return ((unsigned long)get_random_u64()); }
#define	prandom_u32_max(c)	get_random_u32_below(c)
#define	get_random_int()	get_random_u32()

#endif /* _LINUXKPI_INCLUDE_LINUX_RANDOM_H_ */
