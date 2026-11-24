/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation of the Linux ktime_t API from its documented
 * behaviour; function set cross-checked against FreeBSD's LinuxKPI
 * ktime.h (BSD-2-Clause).
 *
 * ktime_t is signed nanoseconds. Clocks: CLOCK_MONOTONIC, BOOTTIME and
 * MONOTONIC_RAW all read gethrtime(9F) (monotonic since boot; illumos
 * does not stop it across suspend, so boottime == monotonic); REALTIME
 * reads gethrestime(9F).
 */
#ifndef _LINUXKPI_LINUX_KTIME_H_
#define	_LINUXKPI_LINUX_KTIME_H_

#include <linuxkpi/platform.h>
#include <linux/types.h>
#include <linux/time64.h>
#include <linux/jiffies.h>
#include <linux/math64.h>

typedef s64 ktime_t;

#define	ktime_set(s, ns)	((ktime_t)(s) * NSEC_PER_SEC + (ktime_t)(ns))
#define	ktime_add(a, b)		((ktime_t)((a) + (b)))
#define	ktime_sub(a, b)		((ktime_t)((a) - (b)))
#define	ktime_add_ns(k, ns)	((ktime_t)((k) + (ns)))
#define	ktime_sub_ns(k, ns)	((ktime_t)((k) - (ns)))
#define	ktime_add_us(k, us)	ktime_add_ns(k, (s64)(us) * NSEC_PER_USEC)
#define	ktime_sub_us(k, us)	ktime_sub_ns(k, (s64)(us) * NSEC_PER_USEC)
#define	ktime_add_ms(k, ms)	ktime_add_ns(k, (s64)(ms) * NSEC_PER_MSEC)
#define	ktime_sub_ms(k, ms)	ktime_sub_ns(k, (s64)(ms) * NSEC_PER_MSEC)
#define	ktime_add_safe(a, b)	ktime_add(a, b)

static inline s64 ktime_to_ns(ktime_t k) { return (k); }
static inline ktime_t ns_to_ktime(u64 ns) { return ((ktime_t)ns); }
static inline ktime_t us_to_ktime(u64 us) { return ((ktime_t)(us * NSEC_PER_USEC)); }
static inline ktime_t ms_to_ktime(u64 ms) { return ((ktime_t)(ms * NSEC_PER_MSEC)); }
static inline s64 ktime_to_us(ktime_t k) { return (div_s64(k, NSEC_PER_USEC)); }
static inline s64 ktime_to_ms(ktime_t k) { return (div_s64(k, NSEC_PER_MSEC)); }
static inline s64 ktime_divns(ktime_t k, s64 div) { return (k / div); }

static inline s64
ktime_us_delta(ktime_t later, ktime_t earlier)
{
	return (ktime_to_us(ktime_sub(later, earlier)));
}

static inline s64
ktime_ms_delta(ktime_t later, ktime_t earlier)
{
	return (ktime_to_ms(ktime_sub(later, earlier)));
}

static inline int
ktime_compare(ktime_t a, ktime_t b)
{
	return (a < b ? -1 : (a > b ? 1 : 0));
}

static inline bool ktime_after(ktime_t a, ktime_t b) { return (a > b); }
static inline bool ktime_before(ktime_t a, ktime_t b) { return (a < b); }
static inline bool ktime_is_zero(ktime_t k) { return (k == 0); }

static inline struct timespec64
ktime_to_timespec64(ktime_t k)
{
	return (ns_to_timespec64(k));
}

static inline ktime_t
timespec64_to_ktime(struct timespec64 ts)
{
	return (ktime_set(ts.tv_sec, ts.tv_nsec));
}

static inline ktime_t
ktime_get(void)
{
	return ((ktime_t)lkpi_plat_nsec());
}

static inline ktime_t
ktime_get_real(void)
{
	int64_t s;
	long ns;

	lkpi_plat_realtime(&s, &ns);
	return (ktime_set(s, ns));
}

#define	ktime_get_raw()			ktime_get()
#define	ktime_get_boottime()		ktime_get()
#define	ktime_get_clocktai()		ktime_get_real()
#define	ktime_get_ns()			((u64)ktime_get())
#define	ktime_get_raw_ns()		((u64)ktime_get())
#define	ktime_get_mono_fast_ns()	((u64)ktime_get())
#define	ktime_get_raw_fast_ns()		((u64)ktime_get())
#define	ktime_get_boottime_ns()		((u64)ktime_get())
#define	ktime_get_real_ns()		((u64)ktime_get_real())
#define	ktime_get_coarse()		ktime_get()
#define	ktime_get_coarse_ns()		((u64)ktime_get())
#define	ktime_get_real_seconds()	((time64_t)(ktime_get_real() / NSEC_PER_SEC))
#define	ktime_get_seconds()		((time64_t)(ktime_get() / NSEC_PER_SEC))
#define	ktime_get_boottime_seconds()	ktime_get_seconds()
#define	ktime_get_ts64(ts)		(*(ts) = ktime_to_timespec64(ktime_get()))
#define	ktime_get_real_ts64(ts)		(*(ts) = ktime_to_timespec64(ktime_get_real()))
#define	ktime_get_raw_ts64(ts)		ktime_get_ts64(ts)
#define	ktime_get_boottime_ts64(ts)	ktime_get_ts64(ts)

#endif /* _LINUXKPI_LINUX_KTIME_H_ */
