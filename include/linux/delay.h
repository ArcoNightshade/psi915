/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation of the Linux delay API from its documented
 * behaviour.
 *
 *   udelay/ndelay/mdelay   busy-wait (drv_usecwait(9F)); any context
 *   msleep/usleep_range/   sleep at least the given time on the
 *   fsleep/ssleep          high-resolution callout (not rounded up to a
 *                          clock tick: illumos hz is 100 by default, and
 *                          i915 polls registers with usleep_range(10, 50))
 *   msleep_interruptible   sleeps in jiffies; returns the ms left when a
 *                          signal ends the sleep early
 */
#ifndef _LINUXKPI_LINUX_DELAY_H_
#define	_LINUXKPI_LINUX_DELAY_H_

#include <linuxkpi/platform.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/jiffies.h>
#include <linux/time64.h>
#include <linux/sched.h>

#define	MAX_UDELAY_MS	5

static inline void
udelay(unsigned long us)
{
	lkpi_plat_udelay(us);
}

static inline void
ndelay(unsigned long ns)
{
	lkpi_plat_udelay((ns + 999) / 1000);
}

static inline void
mdelay(unsigned long ms)
{
	lkpi_plat_udelay(ms * 1000UL);
}

static inline void
msleep(unsigned int ms)
{
	lkpi_plat_sleep_ns((u64)ms * NSEC_PER_MSEC);
}

static inline void
usleep_range(unsigned long min, unsigned long max)
{
	(void) max;
	lkpi_plat_sleep_ns((u64)min * NSEC_PER_USEC);
}

#define	usleep_range_state(min, max, state)	usleep_range(min, max)
#define	usleep_idle_range(min, max)		usleep_range(min, max)

static inline void
ssleep(unsigned int s)
{
	msleep(s * 1000U);
}

static inline void
fsleep(unsigned long us)
{
	if (us <= 10)
		udelay(us);
	else
		lkpi_plat_sleep_ns((u64)us * NSEC_PER_USEC);
}

static inline unsigned long
msleep_interruptible(unsigned int ms)
{
	long left = (long)msecs_to_jiffies(ms);

	while (left > 0 && !signal_pending(current))
		left = schedule_timeout_interruptible(left);
	return (left > 0 ? jiffies_to_msecs((unsigned long)left) : 0);
}

#endif /* _LINUXKPI_LINUX_DELAY_H_ */
