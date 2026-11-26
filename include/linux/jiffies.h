/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation from the documented behaviour of the Linux
 * kernel API; cross-checked against FreeBSD's LinuxKPI jiffies.h
 * (BSD-2-Clause).
 *
 * jiffies is the platform tick clock: illumos lbolt (ddi_get_lbolt64) at
 * hz ticks per second in the kernel, a 1000 Hz monotonic clock in
 * userland. HZ is therefore a run-time value on illumos (hz is 100 by
 * default, 1000 with hires_tick); code that needs HZ in #if or in a
 * static initialiser will not compile.
 */
#ifndef _LINUXKPI_LINUX_JIFFIES_H_
#define	_LINUXKPI_LINUX_JIFFIES_H_

#include <linuxkpi/platform.h>
#include <linux/types.h>
#include <linux/limits.h>

#define	HZ			((int)lkpi_plat_hz())
#define	jiffies			lkpi_plat_ticks()
#define	jiffies_64		((u64)lkpi_plat_ticks())

static inline u64
get_jiffies_64(void)
{
	return ((u64)lkpi_plat_ticks());
}

#define	MAX_JIFFY_OFFSET	((LONG_MAX >> 1) - 1)
#ifndef MAX_SCHEDULE_TIMEOUT
#define	MAX_SCHEDULE_TIMEOUT	LONG_MAX
#endif

/* Wrap-safe comparisons of jiffies values a and b. */
#define	time_after(a, b)	((long)((b) - (a)) < 0)
#define	time_before(a, b)	time_after(b, a)
#define	time_after_eq(a, b)	((long)((a) - (b)) >= 0)
#define	time_before_eq(a, b)	time_after_eq(b, a)
#define	time_in_range(a, b, c)	(time_after_eq(a, b) && time_before_eq(a, c))
#define	time_in_range_open(a, b, c) (time_after_eq(a, b) && time_before(a, c))
#define	time_after64(a, b)	((s64)((b) - (a)) < 0)
#define	time_before64(a, b)	time_after64(b, a)
#define	time_after_eq64(a, b)	((s64)((a) - (b)) >= 0)
#define	time_before_eq64(a, b)	time_after_eq64(b, a)
#define	time_is_after_jiffies(a)	time_before(jiffies, a)
#define	time_is_before_jiffies(a)	time_after(jiffies, a)
#define	time_is_after_eq_jiffies(a)	time_before_eq(jiffies, a)
#define	time_is_before_eq_jiffies(a)	time_after_eq(jiffies, a)

static inline unsigned long
__lkpi_to_jiffies(u64 n, u64 per_sec)
{
	u64 hz = lkpi_plat_hz();
	u64 j;

	/* Round up, so a non-zero interval never becomes zero ticks. */
	if (n > (u64)MAX_JIFFY_OFFSET * per_sec / hz)
		return (MAX_JIFFY_OFFSET);
	j = (n * hz + per_sec - 1) / per_sec;
	return ((unsigned long)j);
}

static inline unsigned long
msecs_to_jiffies(unsigned int m)
{
	return (__lkpi_to_jiffies(m, 1000ULL));
}

static inline unsigned long
usecs_to_jiffies(unsigned int u)
{
	return (__lkpi_to_jiffies(u, 1000000ULL));
}

static inline unsigned long
nsecs_to_jiffies(u64 n)
{
	return (__lkpi_to_jiffies(n, 1000000000ULL));
}

#define	nsecs_to_jiffies64(n)	((u64)nsecs_to_jiffies(n))
#define	secs_to_jiffies(s)	((unsigned long)(s) * lkpi_plat_hz())

static inline unsigned int
jiffies_to_msecs(unsigned long j)
{
	return ((unsigned int)((u64)j * 1000ULL / lkpi_plat_hz()));
}

static inline unsigned int
jiffies_to_usecs(unsigned long j)
{
	return ((unsigned int)((u64)j * 1000000ULL / lkpi_plat_hz()));
}

static inline u64
jiffies_to_nsecs(unsigned long j)
{
	return ((u64)j * (1000000000ULL / lkpi_plat_hz()));
}

#define	jiffies64_to_nsecs(j)	jiffies_to_nsecs((unsigned long)(j))
#define	jiffies64_to_msecs(j)	((u64)jiffies_to_msecs((unsigned long)(j)))

/* Timer-slack helpers: no batching here, so identity. */
#define	round_jiffies(j)		(j)
#define	round_jiffies_up(j)		(j)
#define	round_jiffies_relative(j)	(j)
#define	round_jiffies_up_relative(j)	(j)

#define	time_after32(a, b)	((s32)((u32)(b) - (u32)(a)) < 0)
#define	time_before32(b, a)	time_after32(a, b)

#endif /* _LINUXKPI_LINUX_JIFFIES_H_ */
